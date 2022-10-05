#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <memory.h>

#include "lfqueue.h"
static int _lfqueue_enqueue(struct lfqueue *queue, const void *data)
{
    struct lfqueue_item *tail;
    struct lfqueue_item *item = (void *)((char *)data + queue->_off);
    atomic_store(&item->queue_id, (uintptr_t)&queue->head);
    atomic_store(&item->next, (uintptr_t)NULL);
    tail =
        (struct lfqueue_item *)atomic_exchange(&queue->tail, (uintptr_t)item);
    atomic_store(&tail->next, (uintptr_t)item);
    return 0;
}

static int _lfqueue_dequeue(struct lfqueue *queue, void **data)
{
    struct lfqueue_item *item;
    uintptr_t expected;
    item = (struct lfqueue_item *)atomic_exchange(&queue->head.next,
                                                  (uintptr_t)&queue->head);
    if (item == &queue->head) {
        return -1;
    }
    expected = (uintptr_t)item;
    if (atomic_compare_exchange_strong(&queue->tail, &expected,
                                       (uintptr_t)&queue->head) == false) {
        do {
            expected = atomic_load(&item->next);
        } while (expected == (uintptr_t)NULL);
        atomic_store(&queue->head.next, expected);
    }
    atomic_store(&item->queue_id, (uintptr_t)NULL);
    *data = (void *)((char *)item - queue->_off);
    return 0;
}

static void _lfqueue_fini(struct lfqueue *queue) { free(queue); }

static void _lfqueue_poll(struct lfqueue *queue, void (*fn)(void *, void *),
                          void *carry)
{
    struct lfqueue_item *item, *next, *tail;
    item = (struct lfqueue_item *)atomic_exchange(&queue->head.next,
                                                  (uintptr_t)&queue->head);
    if (item == &queue->head) {
        return;
    }

    tail = (struct lfqueue_item *)atomic_exchange(&queue->tail,
                                                  (uintptr_t)&queue->head);
    atomic_store(&tail->next, (uintptr_t)&queue->head);
    while (item != &queue->head) {
        do {
            next = (void *)atomic_load(&item->next);
        } while (next == NULL);
        atomic_store(&item->queue_id, (uintptr_t)NULL);
        fn((void *)((char *)item - queue->_off), carry);
        item = next;
    }
}

static struct lfqueue_item *_lfqueue_fetch(struct lfqueue *queue)
{
    struct lfqueue_item *item, *tail;
    item = (struct lfqueue_item *)atomic_exchange(&queue->head.next,
                                                  (uintptr_t)&queue->head);
    if (item == &queue->head) {
        return &queue->head;
    }
    tail = (struct lfqueue_item *)atomic_exchange(&queue->tail,
                                                  (uintptr_t)&queue->head);
    atomic_store(&tail->next, (uintptr_t)&queue->head);
    return item;
}

static void *_lfqueue_next(struct lfqueue *queue, struct lfqueue_item **p_it)
{
    struct lfqueue_item *item = *p_it, *next;
    if (item == &queue->head) {
        return NULL;
    }
    do {
        next = (void *)atomic_load(&item->next);
    } while (next == NULL);
    atomic_store(&item->queue_id, (uintptr_t)NULL);
    *p_it = next;
    return (void *)((char *)item - queue->_off);
}

static bool _lfqueue_empty(struct lfqueue *queue)
{
    return atomic_load(&queue->head.next) == (uintptr_t)&queue->head;
}

static bool _lfqueue_inside(struct lfqueue *queue, const void *data)
{
    struct lfqueue_item *item = (void *)((char *)data + queue->_off);
    return atomic_load(&item->queue_id) == (uintptr_t)&queue->head;
}

static struct lfqueue_ops _lfqueue_ops = {.fini = &_lfqueue_fini,
                                          .enqueue = &_lfqueue_enqueue,
                                          .dequeue = &_lfqueue_dequeue,
                                          .poll = &_lfqueue_poll,
                                          .fetch = &_lfqueue_fetch,
                                          .next = &_lfqueue_next,
                                          .empty = &_lfqueue_empty,
                                          .inside = &_lfqueue_inside};

int lfqueue_init(struct lfqueue **queue, size_t off)
{
    *queue = malloc(sizeof(**queue));
    if (*queue == NULL) {
        return -1;
    }
    memset(*queue, 0, sizeof(**queue));
    atomic_store(&(*queue)->head.next, (uintptr_t) & (*queue)->head);
    (*queue)->_off = off;
    (*queue)->tail = (uintptr_t) & (*queue)->head;
    (*queue)->ops = &_lfqueue_ops;
    return 0;
}
