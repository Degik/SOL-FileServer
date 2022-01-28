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
#include <pthread.h>
#include <stdarg.h>

#define BUFF_MESSAGE 256

int isNumber(char *string){
    if(string == NULL){
        return 1;
    }
    if(strlen(string) == 0){
        return 1;
    }
    char *pEnd = NULL;
    errno = 0;
    strtol(string, &pEnd, 10); // Converte una stringa in long integer
    if(errno == ERANGE) {
        return 2;
    }
    if(pEnd != NULL && *pEnd == (char)0){
        return 1;
    }
    return 0;
}

void pthreadMutexLock(pthread_mutex_t *mutex){
    int err;
    if((err = pthread_mutex_lock(mutex)) != 0){
        errno = err;
        perror("pthreadMutexLock");
        exit(EXIT_FAILURE);
    }
}

void pthreadMutexUnlock(pthread_mutex_t *mutex){
    int err;
    if((err = pthread_mutex_unlock(mutex)) != 0){
        errno = err;
        perror("pthreadMutexUnlock");
        exit(EXIT_FAILURE);
    }
}

int printLog(FILE *fileLog, pthread_mutex_t logMutex, const char *message, ...){
    if(message == NULL || fileLog == NULL){
        errno = EINVAL;
        return -1;
    }
    time_t timeDate = time(NULL);
    struct tm* tmInfo = localtime(&timeDate);
    char dateTime[20];
    strftime(dateTime, sizeof(dateTime), "%d-%m-%Y %H:%M:%S", tmInfo);

    // Costruisce il messaggio
    char buffMessage[BUFF_MESSAGE];
    memset(buffMessage, 0, BUFF_MESSAGE);
    va_list args;
    va_start(args, message);
    vsprintf(buffMessage, message, args);
    va_end(args);
    if(pthread_mutex_lock(&logMutex) != 0){
        perror("pthread_mutex_lock");
        return -1;
    }
    fprintf(fileLog, "%s | %s\n", dateTime, buffMessage);
    if(pthread_mutex_unlock((&logMutex)) != 0){
        perror("pthread_mutex_unlock");
        return -1;
    }
    return 1;
}