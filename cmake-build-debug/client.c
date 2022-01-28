#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/un.h>
#include <ctype.h>
#include "includes/util.h"
#include "includes/list.h"
#include "includes/api.h"
#include "includes/parser.h"

#define SIZE_BUFF 8192
#define O_CREATE 1
#define O_OPEN 0

long fileDescriptorSocket;

int setTime(struct timespec *time, int msec);
static int setCurrentTime(long sec, long nsec, struct timespec *time);
int sendCommand(const char *pathname, char command);

int openConnection(const char* sockname, int msec, const struct timespec abstime){
    if(sockname == NULL || msec < 0){
        errno = EINVAL; // Invalid Argument (22)
        return -1;
    }
    //https://stackoverflow.com/questions/3689925/struct-sockaddr-un-vs-sockaddr
    struct sockaddr_un serverAddress; // Unix socket address
    // AF_UNIX file unix presenti sulla stessa macchina
    // SOCK_STREAM TCP Socket
    SYSCALL_EXIT("socket", fileDescriptorSocket, socket(AF_UNIX,SOCK_STREAM,0), "socket");
    // Setta i primi sizeof(serverAddress) bytes del blocco di memoria puntato da serverAddress sul valore specificato
    memset(&serverAddress, '0', sizeof(serverAddress));
    serverAddress.sun_family = AF_UNIX;
    // Vado ad impostare il serverAddress.sunPath, per farlo copio la stringa già presente nel socketNameConfig
    strncpy(serverAddress.sun_path, socketNameConfig, strlen(serverAddress.sun_path) + 1);
    // Aggiungo il carattere di terminazione
    serverAddress.sun_path[strlen(socketNameConfig) + 1] = '\0';
    struct timespec time;
    // Richiamo una funzione esterna per settare il time con il msec
    setTime(&time, msec);
    struct timespec currentTime;
    clock_gettime(CLOCK_REALTIME, &currentTime);
    int error = -1;
    // Provo a connettermi, effettuo il casting su sockaddr
    while((error = connect(fileDescriptorSocket, // int fd_sock
                        (struct sockaddr*)&serverAddress, // const struct sockaddr *sa
                                sizeof(serverAddress))) == -1
                                            && currentTime.tv_nsec < abstime.tv_nsec){

        if(nanosleep(&time, NULL) == -1) {
            fileDescriptorSocket = -1;
            return -1;
        }
        if(clock_gettime(CLOCK_REALTIME, &currentTime) == -1) {
            fileDescriptorSocket = -1;
            return -1;
        }

        if(error == -1){
            fileDescriptorSocket = -1;
            errno = ETIMEDOUT;
            return -1;
        }

        if(activateStandard){
            fprintf(stdout, "Connessione al server riuscita");
        }
        return 0;
    } // Fine while
    return 0;
} // openConnection

int closeConnection(const char* sockname){
    if(sockname == NULL){
        errno = EINVAL;
        return -1;
    }
    if(close(fileDescriptorSocket) == -1){
        fileDescriptorSocket = -1;
        return -1;
    }
    if(activateStandard){
        fprintf(stderr, "Connessione chiusa");
    }
    fileDescriptorSocket = -1;
    return 0;
}// closeConnection

int openFile(const char *pathname, int flags){
    if(pathname == NULL || flags != 1 && flags != 0){
        errno = EINVAL;
        return -1;
    }
    int error;
    error = sendCommand(pathname, 'o');
    if(error != -1){
        errno = EPERM; //Si è tentato di eseguire un'operazione limitata ai processi
        return -1;
    }
    int use;
    SYSCALL_EXIT("safeWrite", use, safeWrite(fileDescriptorSocket, &flags, sizeof(int)), "write");
    int result;
    SYSCALL_EXIT("safeRead", use, safeRead(fileDescriptorSocket, &result, sizeof(int)), "read");
    if(result == 0){
        if(activateStandard) {
            fprintf(stdout, "File aperto o creato\n");
        }
        return 0;
    } else if(result == -2){
        fprintf(stderr, "Esiste già un file con questo nome: %s\n", pathname);
        return -1;
    } else {
        errno = EACCES; // 13 Permesso negato
        fprintf(stderr, "Permesso negato");
        return result;
    }
}// openFile

int readFile(const char* pathname, void** buff, size_t* size){
    if(pathname == NULL){
        errno = EINVAL;
        return -1;
    }
    int error, len, use;
    error = sendCommand(pathname, 'r');
    if(error != -1){
        errno = EPERM; //Si è tentato di eseguire un'operazione limitata ai processi
        return -1;
    }
    SYSCALL_EXIT("safeRead", use, safeRead(fileDescriptorSocket, &len, sizeof(int)), "read");
    *size = len;
    if(*size == -1){
        errno = EPERM;
        fprintf(stderr, "readFIle fallita");
        *buff = NULL;
        *size = 0;
        return -1;
    } else {
        is_null(*buff = malloc(sizeof(char) * len), "malloc");
        SYSCALL_EXIT("safeRead", use, safeRead(fileDescriptorSocket, *buff, sizeof(char) * len), "read");
        if(activateStandard){
            fprintf(stdout, "File letto con successo");
        }
        return 0;
    }
}// readFile

int readNFiles(int N, const char* dirname){
    if(dirname == NULL){
        errno = EINVAL;
        return -1;
    }
    char *number;
    is_null(number = malloc(sizeof(char) * 20), "malloc");
    if(sprintf(number, "%d", N) < 0){
        perror("snprintf");
        return -1;
    }
    int error, use;
    error = sendCommand(number, 'R');
    if(error == -1){
        errno = EPERM;
        return -1;
    }
    free(number);
    SYSCALL_EXIT("safeRead", use, safeRead(fileDescriptorSocket, &N, sizeof(int)), "read");
    int lenPath;
    char *name;
    char **arrayName;
    is_null(arrayName = malloc(sizeof (char*) * N), "malloc");
    int i;
    for(i = 0; i < N; i++){
        SYSCALL_EXIT("safeRead", use, safeRead(fileDescriptorSocket, &lenPath, sizeof(int)), "read"); // Leggo la lunghezza
        is_null((name = malloc(sizeof(char) * (lenPath + 1))), "malloc"); // Allocazione di name
        SYSCALL_EXIT("safeRead", use, safeRead(fileDescriptorSocket, name, lenPath * sizeof(char)), "read"); // Leggo il name
        name[lenPath + 1] = '\0';
        arrayName[i] = name;
    }
    for(i = 0; i < N; i++){
        void *buff;
        size_t buffSize;
        if(openFile(arrayName[i], O_OPEN) == -1){
            errno = EACCES;
            fprintf(stderr, "Errore openFile");
            return -1;
        } else {
            if(readFile(arrayName[i], &buff, &buffSize) == -1) {
                return -1;
            } else {
                char path[SIZE_BUFF];
                if(snprintf(path, sizeof(path), "../%s/%s", dirname, arrayName[i]) < 0){
                    perror("snprint");
                    return -1;
                }
                int fileDescriptor;
                ec_meno1((fileDescriptor = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644)), "open");
                ec_meno1((safeWrite(fileDescriptor, &buff, buffSize)), "safeWrite");
                ec_meno1((close(fileDescriptor)), "close");
                if(closeFile(arrayName[i]) == -1){
                    return -1;
                }
                free(buff);
            }
        }
        free(arrayName[i]);
    }
    free(arrayName);
    return N;
} // readNFiles

int writeFile(const char* pathname){
    if(pathname == NULL){
        errno = EINVAL;
        return -1;
    }
    int error;
    error = sendCommand(pathname, 'W');
    if(error == -1){
        errno = EPERM;
        return -1;
    }
    struct stat status;
    int use, N, result;
    SYSCALL_EXIT("safeRead", use, safeRead(fileDescriptorSocket, &result, sizeof(int)), "read");
    if(result == -1){
        fprintf(stderr, "QUalcosa è andato storto nella writeFile");
        return -1;
    } else {
        ec_meno1(stat(pathname, &status), "stat");
        long size  = (long)status.st_size;
        int fileDescriptorFile = open(pathname, O_RDONLY);
        ec_meno1(fileDescriptorFile, "open");
        char* buff;
        is_null((buff = malloc(sizeof(char) * size)), "malloc");
        ec_meno1((safeRead(fileDescriptorFile, buff, size)), "safeRead");
        ec_meno1((close(fileDescriptorFile)), "close");
        if(appendToFile(pathname, buff, (size_t)size) == -1){
            free(buff);
            return -1;
        }
        free(buff);
        return 0;
    }
} // writeFile

int appendToFile(const char* pathname, void* buff, size_t size){
    if(pathname == NULL){
        errno = EINVAL;
        return -1;
    }
    int use, check, result;
    SYSCALL_EXIT("safeWrite", use, safeWrite(fileDescriptorSocket, &size, sizeof(int)), "write");
    SYSCALL_EXIT("safeRead", use, safeRead(fileDescriptorSocket, &check, sizeof(int)), "read");
    if(!check){
        fprintf(stderr, "Non ho trovato alcun file con questo nome %s", pathname);
        return -1;
    }
    SYSCALL_EXIT("safeWrite", use, safeWrite(fileDescriptorSocket, (char*)buff, sizeof(int) * size), "write");
    SYSCALL_EXIT("safeRead", use, safeRead(fileDescriptorSocket, &result, sizeof(int)), "read");
    if(result == -1){
        errno = EACCES;
        fprintf(stderr, "Errore in appendToFile");
    } else {
        if(activateStandard){
            fprintf(stdout, "appendToFile riuscito correttamente");
        }
        return result;
    }
    return result;
} //appendToFile

int lockFile(const char* pathname){
    if(pathname == NULL){
        errno = EINVAL;
        return -1;
    }
    int error, use, result;
    error = sendCommand(pathname, 'l');
    if(error == -1){
        errno = EPERM;
        return -1;
    }
    SYSCALL_EXIT("safeRead", use, safeWrite(fileDescriptorSocket, &result, sizeof(int)), "read");
    if(result == -1){
        errno = EPERM;
        fprintf(stderr, "lockFile fallita");
        return -1;
    }
    if(activateStandard){
        fprintf(stdout, "lockFile riuscita");
    }
    return 0;
}

int unlockFile(const char* pathname){
    if(pathname == NULL){
        errno = EINVAL;
        return -1;
    }
    int error, use, result;
    error = sendCommand(pathname, 'u');
    if(error == -1){
        errno = EPERM;
        return -1;
    }
    SYSCALL_EXIT("safeRead", use, safeWrite(fileDescriptorSocket, &result, sizeof(int)), "write");
    if(result == -1){
        errno = EPERM;
        fprintf(stderr, "unlockFile fallita");
        return -1;
    }
    if(activateStandard){
        fprintf(stdout, "unlockFile riuscita");
    }
    return 0;
}

int closeFile(const char* pathname){
    if(pathname == NULL){
        errno = EINVAL;
        return -1;
    }
    int error, use, result;
    error = sendCommand(pathname, 'z');
    if(error == -1){
        errno = EPERM;
        return -1;
    }
    SYSCALL_EXIT("safeRead", use, safeRead(fileDescriptorSocket, &result, sizeof(int)), "read");
    if(result == -1){
        errno = EPERM;
        fprintf(stderr, "closeFile fallita");
    } else {
        if(activateStandard){
            fprintf(stdout, "closeFile effettuata");
        }
        return result;
    }
    return result;
} // closeFile

int removeFile(const char* pathname) { // elimina il file dal server
    if(pathname == NULL) {
        errno = EINVAL;
        return -1;
    }
    if(sendCommand(pathname, 'c') == -1) {
        errno = EPERM;
        return -1;
    }
    int result, use;
    SYSCALL_EXIT("safeRead", use, safeRead(fileDescriptorSocket, &result, sizeof(int)), "read");
    if(result == -1) {
        errno = EACCES;
        fprintf(stderr, "removeFile, qualcosa è andato storto\n");
        return -1;
    } else {
        if(activateStandard){
            fprintf(stdout, "File cancellato con successo dal server");
        }
    }
    return 0;
}// removeFile

int execCommand(Command *command){
    if(command == NULL){
        errno = EINVAL;
        return -1;
    }
    switch(command->set){
        case 'c': // Cancello il file command->name dal server
            if(activateStandard){
                fprintf(stdout, "Cancello il file %s", command->name);
            }
            if(openFile(command->name, O_OPEN) != -1){
                if((removeFile(command->name)) == -1){
                    return -1;
                }
            } else {
                return -1;
            }
            break;
        case 'r': // Leggo il file command->name
            if(activateStandard){
                fprintf(stdout, "Ora leggo il file %s", command->name);
            }
            if(openFile(command->name, O_OPEN) != -1){
                void* file;
                size_t fileSize;
                if(readFile(command->name, &file, &fileSize) == -1){
                    return -1;
                } else {
                    if(savePath != NULL && file != NULL){
                        char path[SIZE_BUFF];
                        if(snprintf(path, sizeof(path), "../%s/%s", savePath, command->name) < 0){
                            perror("snprinf");
                            return -1;
                        }
                        int fileDescriptor;
                        ec_meno1((fileDescriptor = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644)), "open");
                        ec_meno1((safeWrite(fileDescriptor, file, fileSize)), "safeWrite");
                        ec_meno1((close(fileDescriptor)), "close");
                        if(closeFile(command->name) == -1){
                            return -1;
                        }
                    }
                    free(file);
                }
            } else {
                return -1;
            }
            break;
        case 'R':
            if(activateStandard){
                fprintf(stdout, "Sto per leggere %d file", command->n);
            }
            if(readNFiles(command->n, savePath) == -1){
                return -1;
            }
            break;
        case 'W': // Scrivo un file sul server
            if(activateStandard){
                fprintf(stdout, "Scrivo il file %s sul server", command->name);
            }
            if(openFile(command->name, O_CREATE) != 1){
                if(writeFile(command->name) == -1){
                    return -1;
                }
                if(closeFile(command->name) == -1){
                    return -1;
                }
            } else if (openFile(command->name, O_OPEN) != -1){
                if(writeFile(command->name) == -1){
                    return -1;
                }
                if(closeFile(command->name) == -1){
                    return -1;
                }
            } else {
                return -1;
            }
            break;
        default:
            if(activateStandard){
                fprintf(stderr, "Attenzione carattere non valido");
            }
            return -1;
    }
    return 0;
}// execCommand

int takeFiles(char* name, int *N, List **L) { //comando 'w' fa una visita ricorsiva per ottenere i file da mandare al server
    if(name == NULL)
        return -1;
    char buffOld[SIZE_BUFF];
    if (getcwd(buffOld, SIZE_BUFF) == NULL) {
        perror("getcwd");
        return -1;
    }
    if (chdir(name) == -1) {
        perror("chdir2");
        fprintf(stderr, "Errore W\n");
        return -1;
    }
    DIR *dir;
    struct dirent *directory;
    if (!(dir = opendir(name))){
        return -1;
    }

    while ((directory = readdir(dir)) != NULL && (*N != 0)) {
        char path[SIZE_BUFF];
        if (directory->d_type == DT_DIR) {
            if (strcmp(directory->d_name, ".") == 0 || strcmp(directory->d_name, "..") == 0 || directory->d_name[0] == '.'){
                continue;
            }
            if(snprintf(path, sizeof(path), "../%s/%s", name, directory->d_name) < 0) {
                perror("snprintf");
                return -1;
            }
            if(takeFiles(path, N, L) == -1){
                return -1;
            }
        } else if (directory->d_type == DT_REG) { //ogni file regolare che vede, deve decrementare n
            if(*N > 0 || *N == -1) { //nuovo file da scrivere
                char *buff;
                is_null((buff = malloc(sizeof(char) * SIZE_BUFF)), "malloc");
                is_null((realpath(directory->d_name, buff)), "realpath");
                Command *command;
                is_null((command = malloc(sizeof(Command))), "malloc");
                command->set = 'W';
                is_null((command->name = malloc(sizeof(char) * (strlen(buff) + 1))), "malloc");
                command->name[strlen(buff) + 1] = '\0';
                command->n = 0;
                strncpy(command->name, buff, strlen(buff));
                if(addHead(L, command) == -1) return -1;
                free(buff);
            }
            if(*N > 0) {
                (*N)--;
            }
        }
    }
    ec_meno1((closedir(dir)), "closedir"); //chiude la directory
    ec_meno1((chdir(buffOld)), "chdir"); //ritorna nella directory vecchia
    return 0;
}// takeFiles

int setTime(struct timespec *time, int msec){
    if(time == NULL || msec < 0){
        errno = EINVAL; // Invalid Argument (22)
        return -1;
    }
    time->tv_sec = msec / 1000;
    msec %= 1000;
    time->tv_nsec = msec * 1000;
    return 0;
}// setTime

static int setCurrentTime(long sec, long nsec, struct timespec *time){
    clock_gettime(CLOCK_REALTIME, time);
    time->tv_sec = time->tv_sec + sec;
    time->tv_nsec = time->tv_nsec + nsec;
    return 0;
}// setCurrentTime

int sendCommand(const char *pathname, char command){
    if(pathname == NULL){
        errno = EINVAL;
        return -1;
    }
    char *text;
    is_null((text = malloc(sizeof(char) * strlen(pathname) + 1)), "malloc");
    strcpy(text, pathname);
    text[strlen(pathname) + 1] = '\0';
    int use;
    SYSCALL_EXIT("safeWrite", use, safeWrite(fileDescriptorSocket, &command, sizeof(char)), "write"); // Evito letture parziali
    int lenText = strlen(text) + 1;
    SYSCALL_EXIT("safeWrite", use, safeWrite(fileDescriptorSocket, &lenText, sizeof(int)), "write");
    SYSCALL_EXIT("safeWrite", use, safeWrite(fileDescriptorSocket, text, lenText * sizeof(char)), "write");
    free(text);
    return 0;
}// sendCommand



int main(int argc, char* argv[]){
    List *L = parser(argc, argv);
    struct sigaction pipe; // Struttura per il nuovo trattamento del segnale
    memset(&pipe, 0, sizeof(struct sigaction));
    pipe.sa_handler = SIG_IGN; // SIG_IGN ignora il segnale
    sigaction(SIGPIPE, &pipe, NULL);
    struct timespec abstime;
    setCurrentTime(10, 0, &abstime);
    ec_meno1((openConnection(socketNameConfig, 1000, abstime)), "openConnection");
    abstime.tv_sec = timeMillis / 1000;
    abstime.tv_nsec = (timeMillis % 1000) * 1000000;
    while (L->size > 0){
        Command *command = removeFirst(&L); // Rimuovo e ritorno il primo elemento della coda
        nanosleep(&abstime, NULL);
        if(activateStandard){
            fprintf(stdout, "Eseguo il comando %c con il parametro %s\n", command->set, command->name);
        }
        if(command->set == 'W'){
            if(command->n == 0){
                command->n = -1;
            }
            if(strcmp(command->name, ".") == 0){
                free(command->name);
                is_null((command->name = malloc(sizeof(char) * SIZE_BUFF)), "malloc");
                is_null((getcwd(command->name, SIZE_BUFF)), "getcwd");
            }
            if(takeFiles(command->name, &(command->n), &L) == -1){
                free(command->name);
                free(command);
                return -1;
            }
            free(command->name);
        } else {
            if(execCommand(command) == -1){
                fprintf(stderr, "Errore di esecuzione del comando %c", command->set);
            }
            free(command->name);
        }
        free(command);
    }
    ec_meno1(closeConnection(socketNameConfig), "closeConnection");
    free(socketNameConfig);
    free(savePath);
    free(L);
    return 0;
}