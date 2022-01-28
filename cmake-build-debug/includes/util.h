#ifndef SOL_GENNAIO_UTIL_H
#define SOL_GENNAIO_UTIL_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/errno.h>

int isNumber(char* string);
void pthreadMutexLock(pthread_mutex_t *mtx);
void pthreadMutexUnlock(pthread_mutex_t *mtx);
int printLog(FILE *fileLog, pthread_mutex_t logMutex, const char *message, ...);

static inline int safeRead(long fd, void *buf, size_t size);
static inline int safeWrite(long fd, void *buf, size_t size);

#define SYSCALL_EXIT(name, r, sc, str, ...)	\
    if ((r=sc) == -1) {				\
	     perror(#name);				\
	      int errno_copy = errno;			\
	       exit(errno_copy);			\
    }

#define is_null(c, s) \
            if((c) == NULL) { \
                perror(s);\
                exit(EXIT_FAILURE); \
            }

#define ec_meno1(c, s) \
        if((c) == -1) {\
            perror(s);     \
            exit(EXIT_FAILURE); \
        }

static inline int safeRead(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left > 0) {
        if ((r = read((int)fd, bufptr, left)) == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (r == 0)
            return 0;
        left-= r;
        bufptr+= r;
    }
    return size;
}

// Evita le scritture parziali
static inline int safeWrite(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left > 0) {
        if ((r = write((int)fd, bufptr, left)) == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (r == 0)
            return 0;
        left-= r;
        bufptr+= r;
    }
    return 1;
}

#endif //SOL_GENNAIO_UTIL_H
