#ifndef SOL_GENNAIO_LIST_H
#define SOL_GENNAIO_LIST_H

#include <pthread.h>

typedef struct Node {
    void *data;
    struct Node *next;
}Node;

typedef struct List {
    Node *head;
    Node *tail;
    unsigned long size;
} List;

List* defaultList();

int addHead(List **L, void *element);

int addTail(List **L, void *element);

void* removeFirst(List **L);

void* removeSecond(List **L);

int removeNode(List **L, Node *N);

void* returnFirst(List *L);

#endif //SOL_GENNAIO_LIST_H
