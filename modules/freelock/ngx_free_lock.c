#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "ngx_free_lock.h"

#define CAS(ptr, oldval, newval) \
    __sync_bool_compare_and_swap ((ptr), (olddval), (newval))

/*compare the oldval with the content of ptr point，if they are equal , then assign newval to ptr,return oldval;if they are not equal ，return the content of ptr point。
 */
#define CMPXCHAG(ptr, oldval, newval) \
    __sync_val_compare_and_swap ((ptr), (oldval),  (newval))

#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))


static enum control_status g_flag = CONTRUE;
/*
new_node:new entry needed to be added
head:the head node in the list
Return whether list is empty before adding.
*/
list_node_t*
flock_list_add_head(list_head_t *head,
                    list_node_t *new_node)
{
    list_node_t *first;

    while (CMPXCHAG(&g_flag, CONTRUE, CONFALSE) == TRUE);

    do {
        new_node->next = first = ACCESS_ONCE(head->first);
    } while (CMPXCHAG(&head->first, first, new_node) != first);

    g_flag = CONTRUE;
    return first;
}


/**
 * llist_del_first - delete the first entry of lock-less list
 * @head:       the head for your lock-less list
 * If list is empty, return NULL, otherwise, return the first entry
 * deleted, this is the newest added one.
 * If multiple consumers are needed, please use list_del_all
 * or use lock between consumers.
 */
list_node_t*
flock_del_first(list_head_t *head)
{
    list_node_t *entry, *old_entry, *next;
    entry = head->first;

    for (;;) {

        if (entry == NULL) {
                    return NULL;
        }

        old_entry = entry;
        next = entry->next;
        entry = CMPXCHAG(&head->first, old_entry, next);

        if (entry == old_entry) {
            break;
        }

    }

    return entry;
}


list_node_t*
flock_del_node(list_head_t *head, list_node_t *node)
{
    list_node_t *phead;

    if (head == NULL) {
        ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0,\
                    "FUNC:%s\t,the head of the free-lock list is NULL,\
                     please ensure init it!",__func__);
        return NULL;
    }

    while (CMPXCHAG(&g_flag, CONTRUE, CONFALSE) == TRUE);

    if (head->first == node) {
        phead = flock_del_first(head);
    } else {
        phead = list_remove_node(head->first, node);
    }

    g_flag = CONTRUE;

    return phead;
}


list_node_t*
flock_first_node(list_head_t *head) {

    if (head == NULL) {
        ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0,\
                    "FUNC:%s\t,the head of the free-lock list is NULL,\
                     please ensure init it!",__func__);
        return NULL;
    }
    return  head->first;
}


list_node_t*
list_node_next(list_node_t *node_t) {

    if (node_t != NULL) {
        return node_t->next;
    } else {
        return NULL;
    }
}


list_head_t*
flock_head_init(list_head_t *head) {

    if (head == NULL) {
        ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0,\
                   "FUNC:%s\t,the head of the list is NULL,\
                    please ensure init it!",__func__);
        return NULL;
    }

    head->first = NULL;

    return head;
}


list_node_t*
list_head_init(list_node_t *head) {

    if (head == NULL) {
        ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0,\
                   "FUNC:%s\t,the head of the list is NULL,\
                    please ensure init it!",__func__);
        return NULL;
    }

    head->next = NULL;

    return head;
}


list_node_t*
list_insert_head(list_node_t *head, \
                list_node_t *node) {

    if (head == NULL) {
        ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, \
                   "FUNC:%s\t,the head of the list is NULL,\
                    please ensure init it!",__func__);
        return NULL;
    }

    node->next = head->next;
    head->next = node;

    return head;
}

list_node_t*
list_node_last(list_node_t *head) {

    if (head == NULL) {
        ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, \
                   "FUNC:%s\t,this list is NULL, \
                    please ensure init it!",__func__);
        return NULL;
    }

    list_node_t *node = head->next;

    while (node != NULL) {
        if (node->next != NULL) {
            node = node->next;
            continue;
        }
        return node;
    }

    return NULL;
}


list_node_t*
list_remove_node(list_node_t *head, list_node_t *node) {
    list_node_t *pnode, *qnode;

    if ((head == NULL) || (head->next == NULL) || (node == NULL)) {
        return NULL;
    }

    qnode = head;
    pnode = head->next;

    while (pnode != node) {

        if (pnode->next != NULL) {
            qnode = pnode;
            pnode = pnode->next;
        } else{
            break;
        }
    }

    if (pnode == node) {
        qnode->next = pnode->next;
    } else{
        return NULL;
    }

    return head;

}


flock_queue_t*
flock_queue_init(flock_queue_t *queue) {
    int i = 0;

    if (queue == NULL) {
        return NULL;
    }

    queue->front = 0;
    queue->rear = 0;

    for (i=0; i<QUEUELEN; i++) {
        queue->elepool[i].status = EMPTY;
        queue->elepool[i].flock_node = NULL;
    }

    return queue;
}


flock_node_t* flock_queue_head_node(flock_queue_t *queue) {
    if (queue == NULL) {
        return NULL;
    }

    if (queue->rear == queue->front) {
        return NULL;
    }

    return queue->elepool[queue->front].flock_node;
}


flock_queue_t*
flock_enqueue(flock_queue_t *queue, flock_node_t  *flock_node) {
    if ((queue == NULL) || (flock_node == NULL)) {
        return NULL;
    }

    do {

        if((queue->rear+1)%QUEUELEN == queue->front) {
            return NULL;
        }

        if (flock_node->status == FULL) {
            return queue;
        }

    } while(CMPXCHAG(&(queue->elepool[queue->rear].status), EMPTY, FULL) == EMPTY);

    queue->elepool[queue->rear].flock_node = flock_node;

    flock_node->status = FULL;
    CMPXCHAG(&(queue->rear), queue->rear, (queue->rear+1)%QUEUELEN);

    return queue;
}


flock_node_t*
flock_dequeue(flock_queue_t *queue) {
    flock_node_t *flock_node;

    if (queue == NULL) {
        return NULL;
    }

    do
    {
        if(queue->rear == queue->front)
        {
            return NULL;
        }

    } while(CMPXCHAG(&(queue->elepool[queue->front].status), FULL, EMPTY) == FULL);

    flock_node = queue->elepool[queue->front].flock_node;
    queue->elepool[queue->front].flock_node = NULL;

    if (flock_node == NULL) {
        ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, 0, \
                   "FUNC:%s\t,free queue appear  NULL point!",__func__);
        return NULL;
    }

    flock_node->status = EMPTY;
    CMPXCHAG(&(queue->front), queue->front, (queue->front+1)%QUEUELEN);

    return flock_node;
}
