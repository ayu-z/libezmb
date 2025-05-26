#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "e_queue.h"

int e_queue_empty(e_queue_t *q){
    return (q->top->next == NULL);
}
void e_queue_init(e_queue_t *q, int const_data){
    q->rear = q->top = (e_qnode_t *)malloc(sizeof(e_qnode_t));
    q->rear->next = NULL;
    q->size = 0;
    q->const_data = const_data;
}

int e_queue_size(e_queue_t *q){
    return q->size;
}

void e_queue_destroy(e_queue_t *q){
    if (!q)
        return;
    while (!e_queue_empty(q)){
        void *data = e_queue_pop(q);
        if (!q->const_data){
            free(data);
        }
    }
    free(q->top);
    q->rear = q->top = NULL;
    q->size = 0;
}

void e_queue_clear(e_queue_t *q){
    if (!q)
        return;
    while (!e_queue_empty(q)){
        void *data = e_queue_pop(q);
        if (!q->const_data)
        {
            free(data);
        }
    }
    q->size = 0;
}

void e_queue_push(e_queue_t *q, void *data)
{
    if (NULL == q || NULL == q->rear || NULL == q->top){
        return;
    }
    e_qnode_t *node = (e_qnode_t *)malloc(sizeof(e_qnode_t));
    node->next = NULL;
    node->data = data;
    q->rear->next = node;
    q->rear = node;
    q->size++;
}

void *e_queue_pop(e_queue_t *q){
    if (NULL == q || NULL == q->rear || NULL == q->top || q->top->next == NULL){
        return NULL;
    }

    e_qnode_t *node = q->top->next;
    q->top->next = node->next;

    if (q->rear == node){
        q->rear = q->top;
    }
    void *data = node->data;
    free(node);
    q->size--;
    return data;
}

int e_queue_merge(e_queue_t *dq, e_queue_t *sq)
{
    while (!e_queue_empty(sq)){
        e_queue_push(dq, e_queue_pop(sq));
    }
    return dq->size;
}

void e_queue_foreach(e_queue_t *q, void *arg, e_queue_foreach_callback callback){
    if (e_queue_empty(q)){
        return;
    }
    e_qnode_t *p = q->top->next;
    while (p != NULL){
        callback(arg, p->data);
        p = p->next;
    }
}