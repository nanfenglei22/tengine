#ifndef NGX_FREE_LOCK_LIST_H_INCLUDED_
#define NGX_FREE_LOCK_LIST_H_INCLUDED_

#include <nginx.h>

#define QUEUELEN    5000

typedef struct list_node_s   list_node_t;
typedef struct list_head_s   list_head_t;
typedef struct flock_queue_s flock_queue_t;
typedef struct queue_node_s  queue_node_t;
typedef struct flock_node_s  flock_node_t;


struct list_node_s {
    list_node_t *next;
};

struct list_head_s {
    list_node_t *first;
};

struct queue_node_t;

enum node_status {
    EMPTY = 1,
    FULL
};

enum control_status {
    CONTRUE = 1,
    CONFALSE
};

struct flock_node_s {
    enum node_status   status;
};

struct queue_node_s {
    enum node_status    status;
    flock_node_t       *flock_node;
};

struct flock_queue_s {
    queue_node_t    elepool[QUEUELEN];
    int             front;
    int             rear;
};


flock_node_t* flock_queue_head_node(flock_queue_t *queue);
flock_queue_t* flock_enqueue(flock_queue_t *queue, flock_node_t *node);
flock_node_t* flock_dequeue(flock_queue_t *queue);
flock_queue_t* flock_queue_init(flock_queue_t *queue);

list_node_t *flock_list_del_first(list_head_t *head);
list_node_t *flock_list_add_head(list_head_t *head, list_node_t *new_node);
list_node_t *flock_first_node(list_head_t *head);
list_head_t *flock_head_init(list_head_t *head);
list_node_t *flock_del_node(list_head_t *head, list_node_t *node);

list_node_t *list_node_next(list_node_t *node_t);
list_node_t *list_head_init(list_node_t *head);
list_node_t *list_insert_head(list_node_t *head, list_node_t *node);
list_node_t *list_node_last(list_node_t *head);
list_node_t *list_remove_node(list_node_t *head, list_node_t *node);

#endif
