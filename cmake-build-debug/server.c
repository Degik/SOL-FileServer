#include "includes/list.h"
#include "includes/util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <libgen.h>
#include <sys/un.h>

#define BUFF_SIZE 1024
#define MB_SIZE 1048576
#define STR_SIZE 100
#define MAX_BACK_LOG 32
#define TRUE 1
#define FALSE 0

typedef struct stats {
    int numCacheReplace;
    int numLock;
    int numUnlock;
    double maxMbytes;
    pthread_mutex_t mutexStats;
} statsInfo;

typedef struct file { // File generico memorizzato nella memoria principale
    char* name;
    char* buff;
    long size;
    int isLock;
    pthread_mutex_t lock;
} fileMem;

typedef struct cmdClient {
    char command;
    char *operations;
    long fileDescriptorConnection;
} commandClient;

// Setting
char *LOG_DIR = "./logs/logs.log";
int maxSpace = 0;
int maxNumberFiles = 0;
int numberThreadWorkers = 0;
char* socketName = NULL;
List *clientsList;
List *filesList;

fd_set fileDescriptorSet;
int **pipeArray;
int isSingint; // Se ricevo un segnale SIGINT/SIGQUIT setto su 1
int isSinghup; // Se ricevo un segnale SIGHUP setto su 1
int *pipeSignal; // Pipe dei segnali
static pthread_mutex_t mutexClientsList = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t  mutexFilesList = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condClientList = PTHREAD_COND_INITIALIZER;
pthread_mutex_t logMutex = PTHREAD_MUTEX_INITIALIZER;
statsInfo  *s;
int spaceUsed = 0;
//
FILE *fileLog = NULL;

void freeSock(){
    if(socketName != NULL){
        unlink(socketName);
        free(socketName);
    }
}

void takeFromConfig(char *configFile){
    char *save, *token, *buff;
    int i;
    is_null((buff = malloc(sizeof(char) * BUFF_SIZE)), "malloc");
    FILE *f;
    is_null((f = fopen(configFile, "r")), "fopen");
    while(fgets(buff, BUFF_SIZE, f)) {
        save = NULL; i = 0;
        buff[strlen(buff) - 1] = '\0'; // Inserisco il terminatore
        token = strtok_r(buff, " ", &save);
        char **str;
        is_null((str = malloc(sizeof(char*) * 2)), "malloc");
        while(token) {
            is_null((str[i] = malloc(sizeof(char) * STR_SIZE)), "malloc");
            strcpy(str[i], token);
            token = strtok_r(NULL, " ", &save);
            i++;
        }
        if(strcmp(str[0], "maxSpace") == 0) { //Setto la maxSpace
            if(isNumber(str[1])) {
                maxSpace = atoi(str[1]);
            } else {
                fprintf(stderr, "Server [Errore il valore maxSpace nel configFile deve essere un numero]\n");
                exit(EXIT_FAILURE);
            }
        }
        if(strcmp(str[0], "maxNumberFiles") == 0) { //Setto la maxNumberFiles
            if(isNumber(str[1])) {
                maxNumberFiles = atoi(str[1]);
            } else {
                fprintf(stderr, "Server [Errore il valore maxNumberFiles nel configFile deve essere un numero]\n");
                exit(EXIT_FAILURE);
            }
        }
        if(strcmp(str[0], "socketName") == 0) { //Setto il socketName
            is_null((socketName = malloc(sizeof(char) * (strlen(str[1]) + 1))), "malloc");
            strcpy(socketName, str[1]);
            socketName[strlen(str[1]) + 1] = '\0';
        }
        if(strcmp(str[0], "numThreadWorkers") == 0) { //Setto il numThreadWorkers
            if(isNumber(str[1])) {
                numberThreadWorkers = atoi(str[1]);
            } else {
                exit(EXIT_FAILURE);
            }
        }
        free(str[1]);
        free(str[0]);
        free(str);
    }
    if(fclose(f) != 0){
        perror("fclose");
        exit(EXIT_FAILURE);
    }
    free(buff);
    if(maxSpace <= 0 || maxNumberFiles <= 0 || socketName == NULL || numberThreadWorkers <= 0){
        fprintf(stderr, "Server [Alcuni valori del configFile non sono stati impostati bene]\n");
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "Server [Spazio: %d]\n", maxSpace);
    fprintf(stdout, "Server [Numero file: %d]\n", maxNumberFiles);
    fprintf(stdout, "Server [Socketname: %s]\n", socketName);
    fprintf(stdout, "Server [Numero workers: %d]\n", numberThreadWorkers);
}

static void* signalWork(void* arg){
    if(arg == NULL){
        errno = EINVAL;
        perror("signalWork");
        exit(EXIT_FAILURE);
    }
    sigset_t *sMask = (sigset_t*)arg;
    int signal, use;
    sigwait(sMask, &signal);
    if(signal == SIGINT || signal == SIGQUIT){
        isSingint = 1;
    } else if (signal == SIGHUP){
        isSinghup = 1;
    }
    SYSCALL_EXIT("safeWrite", use, safeWrite(pipeSignal[1], "segnale", 7), "write");
    return NULL;
}

void printFilesList(List *L){
    Node *curr = L->head;
    fileMem *fileInfo;
    float spaceMb = spaceUsed;
    spaceMb /= MB_SIZE;
    fprintf(stdout, "Server:\nLista dei file\n Spazio in uso: %.2fMb\n", spaceMb);
    printLog(fileLog, logMutex, "È stato chiamata la printFilesList");
    while(curr != NULL){
        fileInfo = curr->data;
        fprintf(stdout, "[Nome del file: %s; Dimensioni del file %ld]\n", fileInfo->name, fileInfo->size);
        curr = curr->next;
    }
}

void printCommandList(List *L){
    Node *curr = L->head;
    commandClient *command = NULL;
    while(curr != NULL){
        command = curr->data;
        fprintf(stdout, "[comando: %c; operazione: %s; fileDescriptorConnection: %ld]\n", command->command, command->operations, command->fileDescriptorConnection);
        curr = curr->next;
    }
    printLog(fileLog, logMutex, "È stata chiamata la printCommandList");
}

Node* returnFile(List *L, char *fileName){
    if(L == NULL || fileName == NULL){
        errno = EINVAL;
        perror("returnFile");
        exit(EXIT_FAILURE);
    }
    Node *curr = L->head;
    fileMem *fileInfo;
    while(curr != NULL){
        fileInfo = curr->data;
        if(strcmp(basename(fileName), fileInfo->name) == 0) {
            // basename prende il nome del percorso a cui punta
            // restituisce un puntatore al componente finale del nome del percorso, eliminando tutti i caratteri '/' finali.
            printLog(fileLog, logMutex, "La return file ha recuperato un file");
            return curr;
        }
    }
    printLog(fileLog, logMutex, "La returnFile non ha trovato alcun file");
    return NULL;
}

static void *ThreadExecWorker(void *arg){
    int numThread = *(int*)arg;
    int use;
    while(TRUE){ // Aspetta finché non riceve un segnale
        pthreadMutexLock(&mutexClientsList);
        while (clientsList->size == 0 || !isSingint){
            if(pthread_cond_wait(&condClientList, &mutexClientsList) != 0){
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
        }
        if(isSingint == TRUE){
            pthreadMutexUnlock(&mutexClientsList);
            fprintf(stderr, "Sono il thread (%d) ho ricevuto un segnale\n", numThread);
            printLog(fileLog, logMutex, "Un thread ha appena ricevuto un segnale");
            break;
        }
        commandClient *tmp = removeFirst(&clientsList);
        pthreadMutexUnlock(&mutexClientsList);
        long fileDescriptorConnection = tmp->fileDescriptorConnection;
        char command = tmp->command;
        char *operations = tmp->operations;
        fprintf(stdout, "Server [Connection: %ld, eseguo il comand %c, con i parametri %s", fileDescriptorConnection, command, operations);
        switch(command){ // Si comporta in base al case
            case 'W':
                fprintf(stdout, "Server [Eseguo il comando scrittura]");
                printLog(fileLog, logMutex, "Il server sta eseguendo il comando scrittura");
                int result = 0;
                pthreadMutexLock(&mutexFilesList);
                Node *file = returnFile(filesList, operations);
                pthreadMutexUnlock(&mutexFilesList);
                if(file == NULL){ // Il file non esiste
                    result = -1;
                    SYSCALL_EXIT("safeWriten", use, safeWrite(fileDescriptorConnection, &result, sizeof(int)), "write");
                } else {
                    fileMem *newFile = file->data;
                    pthreadMutexLock(&newFile->lock);
                    if(newFile->isLock != fileDescriptorConnection){ // Il file è esista, ma non è aperto dal dal client
                        result = -1;
                    }
                    SYSCALL_EXIT("safeWriten", use, safeWrite(fileDescriptorConnection, &result, sizeof(int)), "write");
                    if(result != -1){
                        int sizeFile;
                        SYSCALL_EXIT("safeRead", use, safeRead(fileDescriptorConnection, &sizeFile, sizeof(int)), "read");
                        int spaceEnough;
                        if(sizeFile > maxSpace || sizeFile + newFile->size > maxSpace) {
                            fprintf(stderr, "Il file %s è troppo grande\n", newFile->name);
                            if(sizeFile > maxSpace) {
                                removeNode(&filesList, file);
                            }
                            free(newFile->name);
                            free(newFile);
                            spaceEnough = FALSE;
                        }
                        //Scrivo sul socket se il file entra
                        SYSCALL_EXIT("safeWrite", use, safeWrite(fileDescriptorConnection, &spaceEnough, sizeof(int)), "write");
                        if(spaceEnough){ // C'è spazio
                            fileMem *file;
                            pthreadMutexLock(&mutexFilesList);
                            //Update stats
                            pthreadMutexLock(&s->mutexStats);
                            if(spaceUsed + sizeFile > maxSpace){
                                s->numCacheReplace++;
                            }
                            pthreadMutexUnlock(&s->mutexStats);
                            while(spaceUsed + sizeFile > spaceUsed){
                                // ReplaceCache
                                float spaceMb = spaceUsed;
                                spaceMb /= MB_SIZE;
                                fprintf(stderr, "Il server non ha abbastanza spazio %.2f MB", spaceMb);
                                fileMem  *firstFile = returnFirst(filesList);
                                if(newFile != firstFile){
                                    file = removeFirst(&filesList);
                                } else {
                                    file = removeSecond(&filesList);
                                }
                                fprintf(stderr, "Sto rimuovendo il file %s per fare spazio", file->name);
                                spaceUsed = spaceUsed - file->size;
                                free(file->name);
                                if(file->buff != NULL){
                                    free(file->buff);
                                }
                                free(file);
                            }
                            // Occupo lo spazio del server
                            spaceUsed = spaceUsed + sizeFile;
                            pthreadMutexUnlock(&mutexFilesList);
                            char *buffFile;
                            is_null(buffFile = malloc(sizeof(char) * sizeFile), "malloc");
                            SYSCALL_EXIT("safeRead", use, safeRead(fileDescriptorConnection, buffFile, sizeof(char) * sizeFile), "read");
                            if(newFile->buff == NULL){
                                newFile->size = sizeFile;
                                newFile->buff = buffFile;
                            } else {
                                is_null((newFile->buff = realloc(newFile->buff, sizeof(char) * (sizeFile + newFile->size))), "realloc");
                                int i;
                                for(i = 0; i < sizeFile; i++){
                                    newFile->buff[i + newFile->size] = buffFile[i];
                                }
                                newFile->size = newFile->size + sizeFile;
                                free(buffFile);
                            }
                            result = 0;
                            // Update stats
                            pthreadMutexLock(&s->mutexStats);
                            if(spaceUsed > s->maxMbytes){
                                s->maxMbytes = spaceUsed;
                            }
                            pthreadMutexUnlock(&s->mutexStats);
                            printFilesList(filesList);
                            SYSCALL_EXIT("safeWrite", use, safeWrite(fileDescriptorConnection, &result, sizeof(int)), "write");
                        }
                    }
                    pthreadMutexUnlock(&newFile->lock);
                }
                break;
            case 'c':
                fprintf(stdout, "Server [Cancellazione del file]");
                printLog(fileLog, logMutex, "Il server sta eseguendo il comando cancellazione del file %s", operations);
                pthreadMutexLock(&mutexFilesList);
                Node *file1 = returnFile(filesList, operations);
                int result1;
                if(file1 == NULL){
                    pthreadMutexUnlock(&mutexFilesList);
                    fprintf(stderr, "Server [File non trovato]");
                    result1 = -1;
                } else {
                    fileMem *tmpFile = file1->data;
                    if(tmpFile->isLock != fileDescriptorConnection){
                        fprintf(stderr, "Server [file non aperto dal client]\n");
                        result1 = -1;
                    } else {
                        spaceUsed = spaceUsed - tmpFile->size;
                        result = removeNode(&filesList, file1);
                        free(tmpFile->name);
                        if(tmpFile->buff != NULL){
                            free(tmpFile->buff);
                        }
                        free(tmpFile);
                        fprintf(stdout, "File rimosso con successo dal server");
                    }
                    pthreadMutexUnlock(&mutexFilesList);
                }
                printFilesList(filesList);
                SYSCALL_EXIT("safeWrite", use, safeWrite(fileDescriptorConnection, &result1, sizeof(int)), "write");
                break;
            case 'r':
                fprintf(stdout, "Server [Lettura di un file]\n");
                printLog(fileLog, logMutex, "Il server sta eseguendo il comando di lettura del file %s", operations);
                pthreadMutexLock(&mutexFilesList);
                Node *file2 = returnFile(clientsList, operations);
                int result2;
                if(file2 != NULL){
                    fileMem *tmpFile = file2->data;
                    pthreadMutexLock(&tmpFile->lock);
                    pthreadMutexUnlock(&mutexFilesList);
                    if(tmpFile->isLock == fileDescriptorConnection){ // File aperto dal client
                        result2 = tmpFile->size;
                        char *buff = tmpFile->buff;
                        SYSCALL_EXIT("safeWrite", use, safeWrite(fileDescriptorConnection, &result2, sizeof(int)), "write");
                        SYSCALL_EXIT("safeWrite", use, safeWrite(fileDescriptorConnection, buff, sizeof(char) * result2), "write");
                        fprintf(stdout, "Server [File letto con successo]");
                    } else { //
                        result = -1;
                        SYSCALL_EXIT("safeWrite", use, safeWrite(fileDescriptorConnection, &result2, sizeof(int)), "write");
                        fprintf(stderr, "Server [Non riesco a leggere il file, perché non è aperto dal client]");
                    }
                    pthreadMutexUnlock(&tmpFile->lock);
                } else {
                    pthreadMutexUnlock(&mutexFilesList);
                    result2 = -1;
                    SYSCALL_EXIT("safeWrite", use, safeWrite(fileDescriptorConnection, &result2, sizeof(int)), "write");
                    fprintf(stderr, "Server [Non esiste il file]");
                }
                break;
            case 'R': // Legge N files
                fprintf(stdout, "Server [devo leggere %d files]", atoi(operations));
                int numberFiles;
                if(atoi(operations) > filesList->size){
                    numberFiles = filesList->size;
                } else {
                    numberFiles = atoi(operations);
                }
                if(numberFiles <= 0){
                    numberFiles = filesList->size;
                }
                SYSCALL_EXIT("safeWrite", use, safeWrite(fileDescriptorConnection, &numberFiles, sizeof(int)), "write");
                Node *curr = filesList->head;
                fileMem *file3;
                int buffLen;
                char *buff;
                int i;
                for(i = 0; i < numberFiles; i++){
                    file3 = curr->data;
                    is_null((buff = malloc(sizeof(char) * (strlen(file3->name) + 1))), "malloc");
                    strcpy(buff, file3->name);
                    SYSCALL_EXIT("safeWrite", use, safeWrite(fileDescriptorConnection, &buffLen, sizeof(int)), "write");
                    SYSCALL_EXIT("safeWrite", use, safeWrite(fileDescriptorConnection, buff, sizeof(char) * buffLen), "write");
                    free(buff);
                    curr = curr->next;
                }
                break;
            case 'o': // Apro il file
                fprintf(stdout, "Server [Sto aprendo il file]\n");
                printLog(fileLog, logMutex, "Il server sta eseguendo il comando open del file %s", operations);
                int resultOpen, flags;
                pthreadMutexLock(&mutexFilesList);
                Node *fileOpen = returnFile(filesList, operations);
                SYSCALL_EXIT("safeRead", use, safeRead(fileDescriptorConnection, &flags, sizeof(int)), "read");
                if(fileOpen == NULL && flags == 0){ // Il file non esiste
                    resultOpen = -1;
                    fprintf(stderr, "Server [Il file non esiste]\n");
                } else if(fileOpen == NULL && flags == 1){ // Creo ed apro il file
                    if(filesList->size + 1 > maxNumberFiles){ // Devo rispettare la dimensione massima, quindi cancello qualcosa
                        fprintf(stdout, "Server [Sono pieno di file, mi libero un pò]\n");
                        fileMem *tmpFile = removeFirst(&filesList);
                        fprintf(stdout, "Server [Sto elimnando il file %s]\n", tmpFile->name);
                        spaceUsed = spaceUsed - tmpFile->size;
                        free(tmpFile->name);
                        free(tmpFile->buff);
                        free(tmpFile);
                    }
                    fileMem *newFile;
                    is_null((newFile = malloc(sizeof(fileMem))), "malloc");
                    if(pthread_mutex_init(&newFile->lock, NULL) != 0){ // init del newFile
                        perror("pthread_mutex_init");
                        exit(EXIT_FAILURE);
                    }
                    is_null((newFile->name = malloc(sizeof(char) * (strlen(basename(operations)) + 1))), "malloc");
                    strcpy(newFile->name, basename(operations));
                    newFile->name[strlen(basename(operations)) + 1] = '\0';
                    newFile->size = 0;
                    newFile->buff = NULL;
                    newFile->isLock = fileDescriptorConnection;
                    ec_meno1((addTail(&filesList, newFile)), "addTail");
                    resultOpen = 0;
                } else if(fileOpen != NULL && flags == 0){
                    fileMem *tmpFile = fileOpen->data;
                    if(tmpFile->isLock != -1){
                        resultOpen = -1;
                        fprintf(stderr, "Server [È stato inserito un flag errato]\n");
                    } else {
                        tmpFile->isLock = fileDescriptorConnection;
                        resultOpen = 0;
                    }
                } else if(fileOpen != NULL && flags == 1){
                    resultOpen = 2;
                }
                pthreadMutexLock(&mutexFilesList);
                SYSCALL_EXIT("safeWrite", use, safeWrite(fileDescriptorConnection, &resultOpen, sizeof(int)), "write");
                if(resultOpen == 0){
                    fprintf(stdout, "Server [Ho aperto il file con successo]\n");
                } else {
                    fprintf(stderr, "Server [Qualcosa è andato storto]\n");
                }
                break;
            case 'l': // case lock
                fprintf(stdout, "Server [Sto eseguendo la lockFile]");
                printLog(fileLog, logMutex, "Il server sta eseguendo il comando lockFile %s", operations);
                int resultLock;
                pthreadMutexLock(&mutexFilesList);
                Node *fileLock = returnFile(filesList, operations);
                if(fileLock == NULL){
                    resultLock = -1;
                    pthreadMutexUnlock(&mutexFilesList);
                    fprintf(stderr, "Server [File non trovato]");
                } else {
                    fileMem *tmpFile = fileLock->data;
                    pthreadMutexLock(&tmpFile->lock);
                    pthreadMutexUnlock(&mutexFilesList);
                    if(tmpFile->isLock == fileDescriptorConnection){
                        fprintf(stdout, "Server [Lock del file %s da parte del client %ld]\n", tmpFile->name, fileDescriptorConnection);
                        printLog(fileLog, logMutex, "È stata effettuata una lock del file %s dal client %ld", tmpFile->name, fileDescriptorConnection);
                        resultLock = 0;
                        pthreadMutexLock(&s->mutexStats);
                        s->numLock++;
                        pthreadMutexUnlock(&s->mutexStats);
                    } else {
                        errno = EPERM;
                        resultLock = -1;
                        fprintf(stderr, "Server [Lock del file %s fallita]\n", tmpFile->name);
                        printLog(fileLog, logMutex, "Lock del file %s fallita", tmpFile->name);
                    }
                }
                SYSCALL_EXIT("safeWrite", use, safeWrite(fileDescriptorConnection, &resultLock, sizeof(int)), "");
                break;
            case 'u': // case unlock
                fprintf(stdout, "Server [Sto eseguendo la unlockFile]\n");
                printLog(fileLog, logMutex, "Il server sta eseguendo il comando unlockFile %s", operations);
                int resultUnlock;
                pthreadMutexLock(&mutexFilesList);
                Node *fileUnlock = returnFile(filesList, operations);
                if(fileUnlock == NULL){
                    pthreadMutexUnlock(&mutexFilesList);
                    resultUnlock = -1;
                    fprintf(stderr, "Server [File non trovato]");
                } else {
                    fileMem *tmpFile = fileUnlock->data;
                    pthreadMutexUnlock(&mutexFilesList);
                    if(tmpFile->isLock == fileDescriptorConnection){
                        fprintf(stdout, "Server [Unlock del file %s da parte del client %ld]\n", tmpFile->name, fileDescriptorConnection);
                        printLog(fileLog, logMutex, "È stata effettuata una unlock del file %s dal client %ld", tmpFile->name, fileDescriptorConnection);
                        pthreadMutexUnlock(&tmpFile->lock);
                        resultUnlock = 0;
                        pthreadMutexLock(&s->mutexStats);
                        s->numUnlock++;
                        pthreadMutexUnlock(&s->mutexStats);
                    } else {
                        fprintf(stderr, "Server [Unlock del file %s fallita]\n", tmpFile->name);
                        printLog(fileLog, logMutex, "Unlock del file %s fallita (Il client %ld non è il proprietario mutex)", tmpFile->name, fileDescriptorConnection);
                        resultUnlock = -1;
                        errno = EPERM;
                    }
                }
                SYSCALL_EXIT("safeWrite", use, safeWrite(fileDescriptorConnection, &resultUnlock, sizeof(int)), "write");
                break;
            case 'z': // Chiude il file
                pthreadMutexLock(&mutexFilesList);
                Node *fileClose = returnFile(filesList, operations);
                int resultClose;
                if(fileClose == NULL){
                    pthreadMutexUnlock(&mutexFilesList);
                    resultClose = -1;
                } else {
                    fileMem *tmpFile = fileClose->data;
                    pthreadMutexLock(&tmpFile->lock);
                    if(tmpFile->isLock == fileDescriptorConnection){
                        resultClose = -1;
                    } else {
                        tmpFile->isLock = -1; // resetto il flag
                        resultClose = 0;
                    }
                    pthreadMutexUnlock(&tmpFile->lock);
                    pthreadMutexUnlock(&mutexFilesList);
                }
                SYSCALL_EXIT("safeWrite", use, safeWrite(fileDescriptorConnection, &resultClose, sizeof(int)), "write");
                break;
        }
        FD_SET(fileDescriptorConnection, &fileDescriptorSet); // Riaggiungo alla set per poter essere in caso riutilizzata
        SYSCALL_EXIT("safeWrite", use, safeWrite(pipeArray[numThread][1], "finito", 6), "write"); // DA RICONTROLLARE
        free(tmp->operations);
        free(tmp);
    }
    return NULL;
}

int fdMaxUpdate(fd_set fileDescriptorSet, int fdMax){
    int i;
    for(i = (fdMax-1); i >= 0; --i){
        if(FD_ISSET(i, &fileDescriptorSet)){
            return i;
        }
    }
    return -1;
}

int main(int argc, char* argv[]){
    if(argc != 2){
        fprintf(stderr, "Server [Devi passare il configFile come argomento!]\n");
        exit(EXIT_FAILURE);
    }
    int fileDescriptorSocket, numberActive, use;
    takeFromConfig(argv[1]);
    if((fileLog = fopen(LOG_DIR, "w")) == NULL){
        perror("fopen");
        return errno;
    }
    // Setto le statistiche
    is_null((s = malloc(sizeof(statsInfo))), "malloc");
    if(pthread_mutex_init(&s->mutexStats, NULL) != 0){
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }
    pthreadMutexLock(&s->mutexStats);
    s->maxMbytes = 0;
    s->numCacheReplace = 0;
    s->numLock = 0;
    s->numUnlock = 0;
    pthreadMutexUnlock(&s->mutexStats);
    //Setto i segnali
    isSingint = FALSE;
    isSinghup = FALSE;
    // Setto la gestione sigaction
    struct sigaction sigActionInt;
    struct sigaction sigActionQuit;
    struct sigaction sigActionHup;
    printLog(fileLog, logMutex, "Il server ha appena finito di settare la gestione sigaction");
    // Le inizializzo a 0
    memset(&sigActionInt, 0, sizeof(sigActionInt));
    memset(&sigActionQuit, 0, sizeof(sigActionQuit));
    memset(&sigActionHup, 0, sizeof(sigActionHup));
    sigset_t sMask;
    printLog(fileLog, logMutex, "Il server ha appena finito di inizializzare a 0 le sigaction");
    sigemptyset(&sMask); // Azzera la maschera puntata sMask
    // Metto ad 1 la posizione dei segnali SIGINT, SIGQUIT, SIGHUP nel sMask
    sigaddset(&sMask, SIGINT);
    sigaddset(&sMask, SIGQUIT);
    sigaddset(&sMask, SIGHUP);
    if(pthread_sigmask(SIG_SETMASK, &sMask, NULL) != 0){ // SIG_SETMASK la nuova signal mask diventa set a prescindere dal valore di quella vecchia
        perror("pthread_sigmask");
        exit(EXIT_FAILURE);
    }
    pthread_t signalGestor;
    if(pthread_create(&signalGestor, NULL, signalWork, (void*)&sMask) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    is_null(pipeSignal = (int*)malloc(sizeof(int) * 2), "malloc");
    ec_meno1((pipe(pipeSignal)), "pipe");
    clientsList = defaultList();
    filesList = defaultList();
    pthread_t *threadArray;
    is_null((threadArray = malloc(sizeof(pthread_t) * numberThreadWorkers)), "malloc");
    is_null((pipeArray = (int**)malloc(sizeof(int*) * numberThreadWorkers)), "malloc");
    printLog(fileLog, logMutex, "Il server ha appena finito di allocare il threadArray e la pipeArray");
    int *tmpArray;
    is_null((tmpArray = malloc(sizeof(int) * numberThreadWorkers)), "malloc");
    int i;
    for(i = 0; i < numberThreadWorkers; i++){
        tmpArray[i] = i;
        if(pthread_create(&threadArray[i], NULL, ThreadExecWorker, (void*)&(tmpArray[i])) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
        is_null((pipeArray[i] = (int*)malloc(sizeof(int) * 2)), "malloc");
        ec_meno1((pipe(pipeArray[i])), "pipe");
    }
    fileDescriptorSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fileDescriptorSocket == -1){
        perror("socket");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_un serverAddress;
    memset(&serverAddress, '0', sizeof(serverAddress));
    serverAddress.sun_family = AF_UNIX;
    strncpy(serverAddress.sun_path, socketName, strlen(socketName) + 1);
    numberActive = 0;
    SYSCALL_EXIT("bind", use, bind(fileDescriptorSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)), "bind");
    SYSCALL_EXIT("listen", use, listen(fileDescriptorSocket, MAX_BACK_LOG), "listen");
    fd_set tmpSet;
    FD_ZERO(&fileDescriptorSet);
    FD_ZERO(&tmpSet);
    FD_SET(fileDescriptorSocket, &fileDescriptorSet);
    int fdMax = fileDescriptorSocket;
    for(i = 0; i < numberThreadWorkers; i++){
        FD_SET(pipeArray[i][0], &fileDescriptorSet);
        if(pipeArray[i][0] > fdMax){
            fdMax = pipeArray[i][0];
        }
    }
    // Inserisco la pipe dei segnali nel set
    FD_SET(pipeSignal[0], &fileDescriptorSet);
    if(pipeSignal[0] > fdMax){
        fdMax = pipeSignal[0];
    }
    while(!isSingint){
        tmpSet = fileDescriptorSet;
        ec_meno1((select(fdMax+1, &tmpSet, NULL, NULL, NULL)), "select");
        for(i = 0; i <= fdMax; i++){
            if(FD_ISSET(i, &tmpSet)){
                long fileDescriptorConnection;
                if(i == fileDescriptorSocket){
                    ec_meno1((fileDescriptorConnection = accept(fileDescriptorSocket, (struct sockaddr*)NULL, NULL)), "accept");
                    FD_SET(fileDescriptorConnection, &fileDescriptorSet);
                    numberActive++;
                    if(fileDescriptorConnection > fdMax){
                        fdMax = fileDescriptorConnection;
                    }
                    continue;
                }
                fileDescriptorConnection = i;
                if(fileDescriptorConnection == pipeSignal[0]){
                    // Ho ricevuto un segnale
                    char tmpBuff[8];
                    SYSCALL_EXIT("safeRead", use, safeRead(fileDescriptorConnection, tmpBuff, 7), "read");
                    if(isSingint == TRUE || numberActive == 0){
                        isSingint = TRUE;
                        if(pthread_cond_broadcast(&condClientList) != 0){
                            perror("pthread_cond_broadcast");
                            exit(EXIT_FAILURE);
                        }
                    }
                    if(isSinghup){
                        FD_CLR(fileDescriptorSocket, &fileDescriptorSet);
                        ec_meno1((close(fileDescriptorSocket)), "close");
                    }
                    continue;
                }
                int isPipe = FALSE;
                int j;
                for(j = 0; j < numberThreadWorkers; j++){
                    if(fileDescriptorConnection == pipeArray[j][0]){
                        char tmpBuff[7];
                        SYSCALL_EXIT("safeRead", use, safeRead(fileDescriptorConnection, tmpBuff, 6), "read");
                        isPipe = TRUE;
                    }
                }
                if(isPipe){
                    continue;
                }
                FD_CLR(fileDescriptorConnection, &fileDescriptorSet);
                char command;
                int readCommand = safeRead(fileDescriptorConnection, &command, sizeof(char));
                if(readCommand == 0 || readCommand == -1){
                    fprintf(stderr, "Server [Client %ld offline]\n", fileDescriptorConnection);
                    printLog(fileLog, logMutex, "Un client si è disconesso %ld", fileDescriptorConnection);
                    numberActive--;
                    FD_CLR(fileDescriptorConnection, &fileDescriptorSet);
                    ec_meno1((close(fileDescriptorConnection)), "close");
                    if(fileDescriptorConnection == fdMax){
                        fdMax = fdMaxUpdate(fileDescriptorSet, fdMax);
                    }
                    if(numberActive > 0){
                        if(isSinghup){
                            fprintf(stderr, "Server [Connessioni attive: %d]\n", numberActive);
                            printLog(fileLog, logMutex, "Le connessioni attive sono: %d", numberActive);
                        }
                    } else {
                        if(isSinghup){
                            fprintf(stderr, "Server [Non sono presenti connessioni]\n");
                            printLog(fileLog, logMutex, "Non sono presenti connessioni");
                        }
                        if(isSinghup) {
                            isSinghup = TRUE;
                            if(pthread_cond_broadcast(&condClientList) != 0){
                                perror("pthread_cond_broadcast");
                                exit(EXIT_FAILURE);
                            }
                        }
                    }
                    continue;
                }
                if(readCommand < 0){
                    perror("safeRead");
                    exit(EXIT_FAILURE);
                }
                char *operationStr;
                int strLen;
                SYSCALL_EXIT("safeRead", use, safeRead(fileDescriptorConnection, &strLen, sizeof(int)), "read");
                is_null((operationStr = calloc(strLen, sizeof(char))), "calloc");
                SYSCALL_EXIT("safeRead", use, safeRead(fileDescriptorConnection, operationStr, sizeof(char) * strLen), "read");
                commandClient *tmpCommand;
                is_null((tmpCommand = malloc(sizeof(commandClient))), "malloc");
                tmpCommand->command = command;
                is_null((tmpCommand->operations = malloc(sizeof(char) * (strlen(operationStr) + 1))), "malloc");
                tmpCommand->fileDescriptorConnection = fileDescriptorConnection;
                strcpy(tmpCommand->operations, operationStr);
                tmpCommand->operations[strlen(operationStr) + 1] = '\0';
                free(operationStr);
                pthreadMutexUnlock(&mutexClientsList);
                if(pthread_cond_signal(&condClientList) != 0){
                    perror("pthread_cond_signal");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
    for(i = 0; i < numberThreadWorkers; i++){
        // Aspetto che tutti i thread terminino
        if(pthread_join(threadArray[i], NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }
    printLog(fileLog, logMutex, "Tutti i thread sono terminati");
    if(pthread_join(signalGestor, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    printLog(fileLog, logMutex, "Il thread signalGestor è terminato");
    for(i = 0; i < numberThreadWorkers; i++){
        ec_meno1((close(pipeArray[i][0])), "close");
        ec_meno1((close(pipeArray[i][1])), "close");
        FD_CLR(pipeArray[i][0], &fileDescriptorSet);
        free(pipeArray[i]);
    }
    free(pipeArray);
    FD_CLR(pipeSignal[0], &fileDescriptorSet);
    ec_meno1((close(pipeSignal[0])), "close");
    ec_meno1((close(pipeSignal[1])), "close");
    free(pipeSignal);
    for(i = fdMax; i >= 0; --i){
        if(FD_ISSET(i, &fileDescriptorSet)){
            ec_meno1((close(i)), "close");
        }
    }
    freeSock();
    fprintf(stdout, "Server [Ora stampo le statistiche]\n");
    fprintf(stdout, "Server [cacheReplace: %d; maxMbytes %.2fMb]\n", s->numCacheReplace, s->maxMbytes / MB_SIZE);
    printLog(fileLog, logMutex, "Sono state stampate le statistiche [cacheReplace: %d; maxMbytes %.2fMb]", s->numCacheReplace, s->maxMbytes / MB_SIZE);
    printFilesList(filesList);
    free(tmpArray);
    free(threadArray);
    free(s);
    printLog(fileLog, logMutex, "Il server ha appena liberato la memoria da [tmpArray; threadArray; s]");
    fileMem *fileInfo = removeFirst(&clientsList);
    while(fileInfo != NULL){
        free(fileInfo->name);
        free(fileInfo->buff);
        free(fileInfo);
        fileInfo = removeFirst(&clientsList);
    }
    free(filesList);
    printLog(fileLog, logMutex, "Il server ha appena liberato e ripulito la filesList");
    commandClient *command = removeFirst(&clientsList);
    while(command != NULL){
        free(command->operations);
        free(command);
        command = removeFirst(&clientsList);
    }
    free(clientsList);
    printLog(fileLog, logMutex, "Il server ha appena liberato e ripulito la clientsList");

    if(fclose(fileLog) != 0){
        perror("fclose");
    }
    return 0;
}
