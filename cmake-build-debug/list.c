#include <stdio.h>
#include <stdlib.h>
#include "includes/list.h"
#include "includes/util.h"

List* defaultList(){
    List *L;
    is_null(L = malloc(sizeof(List)), "malloc");
    L->head = NULL;
    L->tail = NULL;
    L->size = 0;
    return L;
}

int addHead(List **L, void *element){
    Node* N;
    is_null(N = malloc(sizeof(Node)), "malloc");
    N->data = element;
    N->next = NULL;
    if((*L)->size == 0){
        (*L)->head = N;
        (*L)->tail = N;
        (*L)->size++;
    } else {
        (*L)->head = N;
        (*L)->size++;
    }
    return 0;
}

int addTail(List **L, void *element){
    Node* N;
    is_null(N = malloc(sizeof(Node)), "malloc");
    N->data = element;
    N->next = NULL;
    if((*L)->size == 0){
        (*L)->head = N;
        (*L)->tail = N;
        (*L)->size++;
    } else {
        (*L)->tail->next = N;
        (*L)->tail = N;
        (*L)->size++;
    }
    return 0;
}

void* removeFirst(List **L){
    if((*L)->head == NULL){
        return NULL;
    }
    void *result = (*L)->head->data;
    Node *tmp = (*L)->head;
    if((*L)->head->next == NULL){
        (*L)->tail = NULL;
        (*L)->size = 0;
    } else {
        (*L)->head = (*L)->head->next;
        (*L)->size--;
    }
    free(tmp);
    return result;
}

void* removeSecond(List **L){
    if((*L)->head->next == NULL){
        return NULL;
    }
    void *result = (*L)->head->next->data; // Restituisco il secondo elemento
    Node *tmp = (*L)->head->next;
    if((*L)->head->next->next == NULL){
        (*L)->tail = NULL;
        (*L)->size = 1;
    } else {
        (*L)->head->next = (*L)->head->next->next;
        (*L)->size--;
    }
    free(tmp);
    return result;
}

int removeNode(List **L, Node *N){
    int result = -1;
    Node *tmp = (*L)->head;
    Node *prec = NULL;
    while(tmp != NULL){
        if(N == tmp) {
            (*L)->size--;
            if(prec == NULL){
                (*L)->head = tmp->next;
                if((*L)->size == 0){
                    (*L)->tail = NULL;
                }
            } else {
                prec->next = tmp->next;
                if(tmp->next == NULL){
                    (*L)->tail = prec;
                }
            }
            free(tmp);
            tmp = NULL;
            result = 0;
        }
        if(tmp != NULL){
            prec = tmp;
            tmp = tmp->next;
        }
    }
    return result;
}

void* returnFirst(List *L){
    if(L == NULL){
        return NULL;
    }
    return L->head->data;
}