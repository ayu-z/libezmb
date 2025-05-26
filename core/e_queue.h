
#ifndef __E_QUEUE_H__
#define __E_QUEUE_H__

typedef struct e_qnode{
    void *data;
    struct e_qnode * next;
}e_qnode_t;

typedef struct e_queue{
    struct e_qnode * top;
    struct e_qnode * rear;
    int size;
    int const_data;
}e_queue_t;

typedef int (*e_queue_foreach_callback)(void *arg, void *data);

int e_queue_empty(e_queue_t *q);
void e_queue_init(e_queue_t *q, int const_data);
int e_queue_size(e_queue_t *q);
void e_queue_destroy(e_queue_t *q);
void e_queue_clear(e_queue_t *q);
void e_queue_push(e_queue_t *q, void *data);
void *e_queue_pop(e_queue_t *q);
int e_queue_merge(e_queue_t *dq, e_queue_t *sq);
void e_queue_foreach(e_queue_t *q, void *arg, e_queue_foreach_callback callback);
#endif