#include <memory.h>
#include <stdlib.h>
#include <pthread.h>

#include "queue.h"

#define LOCK_INIT(X) pthread_spin_init(X, 0)
#define LOCK_FINI(X) pthread_spin_destroy(X)
#define LOCK(X) pthread_spin_lock(X)
#define UNLOCK(X) pthread_spin_unlock(X)

static int _queue_enqueue(struct queue *queue, const void *data)
{
    struct queue_item *tail;
    struct queue_item *item = malloc(sizeof(*item));
    if (item == NULL) {
        return -1;
    }
    item->data = data;
    LOCK(&queue->lck);
    tail = queue->tail;
    queue->tail = item;
    tail->next = item;
    UNLOCK(&queue->lck);
    return 0;
}

static int _queue_dequeue(struct queue *queue, void **data)
{
    struct queue_item *item;
    LOCK(&queue->lck);
    item = queue->head.next;
    queue->head.next = item != queue->tail ? item->next : &queue->head;
    queue->tail = item == queue->tail ? &queue->head : queue->tail;
    UNLOCK(&queue->lck);
    if (item == &queue->head) {
        return -1;
    }
    *data = (void *)item->data;
    return 0;
}

static void _queue_fini(struct queue *queue)
{
    struct queue_item *it = queue->head.next, *next;
    while (it && it->next != &queue->head) {
        next = it->next;
        free(it);
        it = next;
    }
    LOCK_FINI(&queue->lck);
    free(queue);
}

static void _queue_poll(struct queue *queue, void (*fn)(void *, void *),
                        void *carry)
{
    struct queue_item *it, *tail, *next;
    LOCK(&queue->lck);
    it = queue->head.next;
    tail = queue->tail;
    queue->head.next = tail->next = queue->tail = &queue->head;
    UNLOCK(&queue->lck);
    if (it == &queue->head) {
        return;
    }
    while (it != &queue->head) {
        do {
            next = it->next;
        } while (next == NULL);
        fn((void *)it->data, carry);
        free(it);
        it = next;
    }
}

static struct queue_ops _queue_ops = {.fini = &_queue_fini,
                                      .enqueue = &_queue_enqueue,
                                      .dequeue = &_queue_dequeue,
                                      .poll = &_queue_poll};

int queue_init(struct queue **queue)
{
    *queue = malloc(sizeof(**queue));
    if (*queue == NULL) {
        return -1;
    }
    memset(*queue, 0, sizeof(**queue));
    (*queue)->head.next = &(*queue)->head;
    (*queue)->tail = &(*queue)->head;
    LOCK_INIT(&(*queue)->lck);
    (*queue)->ops = &_queue_ops;
    return 0;
}
