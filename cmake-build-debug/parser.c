#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <getopt.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <ctype.h>
#include "includes/list.h"
#include "includes/parser.h"
#include "includes/util.h"

int activater;
int activateR;
int activatef;
int activatep;
int activateD
int activatew;
int activateW;
int activateStandard;
int timeMillis;

void printList(List *L){
    Node *tmp = L->head;
    Command *command = NULL;
    while(tmp != NULL){
        command = tmp->data;
        fprintf(stdout , "Comando: %c, %s, %d\n", command->set, command->name, command->n);
        tmp = tmp->next;
    }
}

void createCommand(List **L, char set, char *name, int n){
    Command *newCommand;
    is_null((newCommand = malloc(sizeof(Command))), "malloc");
    newCommand->set = set;
    if(name != NULL){
        is_null((newCommand->name = malloc(sizeof(char) * (strlen(name) + 1))), "malloc");
        strcpy(newCommand->name, name);
        newCommand->name[strlen(name)] = '\0';
    }
    newCommand->n = n;
    ec_meno1(addTail(L,newCommand), "push");
}

List* parser(int argc, char* argv[]){
    int set;
    savePath = NULL;
    activatef = 0;
    activatep = 0;
    activateStandard = 0;
    timeMillis = 0;
    char *token, *save, *tmp;
    char* nextStr;
    int option;
    List *L;
    is_null(L = malloc(sizeof(List)), "malloc");
    L->size = 0;
    while((set = getopt(argc, argv, "hf:w:W:r:Rd:t:l:u:c:p")) != -1){
        switch(set){
            case 'h': {
                fprintf(stdout, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
                        "-h                 : stampa la lista opzioni",
                        "-f filename        : specifica il nome del socket AF_UNIX a cui connettersi",
                        "-w dirname[,n=0]   : invia i file nella cartella dirname al server",
                        "-W file1[,file2]   : lista di nomi di file da scrivere nel server",
                        "-r file1[,file2]   : lista dei file che devono essere letti dal server",
                        "-R [n=0]           : leggi 'n' file presenti sul server",
                        "-d dirname         : specifica la cartella dove scrivere i file letti con -r e -R",
                        "-t time            : tempo in millisecondi per fare due richieste consecutive al server",
                        "-c file1[,file2]   : lista di file da eliminare dal server, se presenti",
                        "-p                 : attiva lo standard output"
                );
                exit(EXIT_SUCCESS);
            }
            case 'f': {
                if(activatef == 1){
                    fprintf(stderr, "Attenzione puoi inserire -f una sola volta\n");
                    exit(EXIT_FAILURE);
                }
                activatef = 1;
                is_null((socketNameConfig = malloc(sizeof(char) * (strlen(optarg) + 1))), "malloc");
                strcpy(socketNameConfig, optarg);
                socketNameConfig[strlen(optarg)] = '\0';
                break;
            }
            case 'w': {
                activatew = 1;
                is_null((tmp = malloc(sizeof(char) * (strlen(optarg) + 1))), "malloc");
                strncpy(tmp, optarg, strlen(optarg));
                tmp[strlen(optarg)] = '\0';
                save = NULL;
                token = strtok_r(tmp, ",", &save);
                char *dirName;
                is_null((dirName = malloc(sizeof(char) * (strlen(token) + 1))), "malloc");
                strcpy(dirName, token);
                dirName[strlen(token)] = '\0';
                int count = -1;
                char *tmp1;
                while (token) {
                    tmp1 = token;
                    token = strtok_r(NULL, ",", &save);
                    count++;
                }
                int number;
                if (count > 1) {
                    fprintf(stderr, "Troppi argomenti\n");
                    exit(EXIT_FAILURE);
                } else if (count == 1) {
                    if (isNumber(tmp1)) {
                        number = atoi(tmp1);
                    } else {
                        fprintf(stderr, "Deve essere un numero!\n");
                        exit(EXIT_FAILURE);
                    }
                } else {
                    number = 0;
                }
                createCommand(&L, 'w', dirName, number);
                free(dirName);
                free(tmp);
                break;
            }
            case 'W': {
                activateW = 1;
                is_null((tmp = malloc(sizeof(char) * (strlen(optarg) + 1))), "malloc");
                strncpy(tmp, optarg, strlen(optarg));
                tmp[strlen(optarg)] = '\0';
                save = NULL;
                token = strtok_r(tmp, ",", &save);
                while (token) {
                    createCommand(&L, 'W', token, 0);
                    token = strtok_r(NULL, ",", &save);
                }
                free(tmp);
                break;
            }
            case 'r': {
                is_null((tmp = malloc(sizeof(char) * (strlen(optarg) + 1))), "malloc");
                strncpy(tmp, optarg, strlen(optarg));
                tmp[strlen(optarg)] = '\0';
                save = NULL;
                token = strtok_r(tmp, ",", &save);
                while (token) {
                    token[strlen(token)] = '\0';
                    createCommand(&L, 'r', token, 0);
                    token = strtok_r(NULL, ",", &save);
                }
                free(tmp);
                activater = 1;
                break;
            }
            case 'R': {
                option = 0;
                nextStr = NULL;
                if (optind != argc) {
                    nextStr = strdup(argv[optind]);
                }
                if (nextStr != NULL && nextStr[0] != '-') {
                    if (isNumber(nextStr)) {
                        option = atoi(nextStr);
                    } else {
                        fprintf(stderr, "Devi inserire un numero -R[n=0]");
                        exit(EXIT_FAILURE);
                    }
                }
                free(nextStr);
                activateR = 1;
                createCommand(&L, 'R', "-", option);
                break;
            }
            case 'd': {
                is_null((savePath = malloc(sizeof(char) * (strlen(optarg) + 1))), "malloc");
                strcpy(savePath, optarg, strlen(optarg));
                savePath[strlen(optarg)] = '\0';
                break;
            }
            case 't': {
                timeMillis = atoi(optarg);
                break;
            }
            case 'l': {
                is_null((tmp = malloc(sizeof(char) * (strlen(optarg) + 1))), "malloc");
                strncpy(tmp, optarg, strlen(optarg));
                tmp[strlen(optarg)] = '\0';
                save = NULL;
                token = strtok_r(tmp, ",", &save);
                while (token) {
                    token[strlen(token)] = '\0';
                    createCommand(&L, 'l', token, 0);
                    token = strtok_r(NULL, ",", &save);
                }
                break;
            }
            case 'u': {
                is_null((tmp = malloc(sizeof(char) * (strlen(optarg) + 1))), "malloc");
                strncpy(tmp, optarg, strlen(optarg));
                tmp[strlen(optarg)] = '\0';
                save = NULL;
                token = strtok_r(tmp, ",", &save);
                while (token) {
                    token[strlen(token)] = '\0';
                    createCommand(&L, 'u', token, 0);
                    token = strtok_r(NULL, ",", &save);
                }
                break;
            }
            case 'c': {
                is_null((tmp = malloc(sizeof(char) * (strlen(optarg) + 1))), "malloc");
                strncpy(tmp, optarg, strlen(optarg));
                tmp[strlen(optarg)] = '\0';
                save = NULL;
                token = strtok_r(tmp, ",", &save);
                while (token) {
                    createCommand(&L, 'r', token, 0);
                    token = strtok_r(NULL, ",", &save);
                }
                free(tmp);
                break;
            }
            case 'p': {
                if (activatep == 1) {
                    fprintf(stderr, "L'opzione va specificata una volta sola\n");
                    exit(EXIT_FAILURE);
                }
                activateStandard = 1;
                activatep = 1;
                break;
            }
            case ':': {
                fprintf(stderr, "L'opzione -%c richiede un argomento\n", optopt);
                break;
            }
            case '?': {
                fprintf(stderr, "Operazione non riconosciuta -%c\n", optopt);
                break;
            }
        }
    }
    if(savePath != NULL
        && activater == 0
            && activateR == 0){
        fprintf(stderr, "Operazione non valida! devi insirere tutte le opzioni\n");
        exit(EXIT_FAILURE);
    }
    if(activateR){
        if(!activatew && !activateW){
            fprintf(stderr, "Operazione non valida, quando usi -D devi usare anche -w o -W\n");
            exit(EXIT_FAILURE);
        }
    }
    return L;
}
