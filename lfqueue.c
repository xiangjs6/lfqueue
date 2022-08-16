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
    atomic_store(&item->next, (uintptr_t)NULL);
    tail =
        (struct lfqueue_item *)atomic_exchange(&queue->tail, (uintptr_t)item);
    atomic_store(&tail->next, (uintptr_t)item);
    return 0;
}

static int _lfqueue_dequeue(struct lfqueue *queue, void **data)
{
    struct lfqueue_item *it;
    uintptr_t expected;
    it = (struct lfqueue_item *)atomic_exchange(&queue->head.next,
                                                (uintptr_t)&queue->head);
    if (it == &queue->head) {
        return -1;
    }
    expected = (uintptr_t)it;
    if (atomic_compare_exchange_strong(&queue->tail, &expected,
                                       (uintptr_t)&queue->head) == false) {
        do {
            expected = atomic_load(&it->next);
        } while (expected == (uintptr_t)NULL);
        atomic_store(&queue->head.next, expected);
    }
    *data = (void *)((char *)it - queue->_off);
    return 0;
}

static void _lfqueue_fini(struct lfqueue *queue) { free(queue); }

static void _lfqueue_poll(struct lfqueue *queue, void (*fn)(void *, void *),
                          void *carry)
{
    struct lfqueue_item *it, *next, *tail;
    it = (struct lfqueue_item *)atomic_exchange(&queue->head.next,
                                                (uintptr_t)&queue->head);
    if (it == &queue->head) {
        return;
    }

    tail = (struct lfqueue_item *)atomic_exchange(&queue->tail,
                                                  (uintptr_t)&queue->head);
    atomic_store(&tail->next, (uintptr_t)&queue->head);
    while (it != &queue->head) {
        do {
            next = (void *)atomic_load(&it->next);
        } while (next == NULL);
        fn((void *)((char *)it - queue->_off), carry);
        it = next;
    }
}

static bool _lfqueue_empty(struct lfqueue *queue)
{
    return atomic_load(&queue->head.next) == (uintptr_t)&queue->head;
}

static struct lfqueue_ops _lfqueue_ops = {.fini = &_lfqueue_fini,
                                          .enqueue = &_lfqueue_enqueue,
                                          .dequeue = &_lfqueue_dequeue,
                                          .poll = &_lfqueue_poll,
                                          .empty = _lfqueue_empty};

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
