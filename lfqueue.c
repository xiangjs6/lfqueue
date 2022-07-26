#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <memory.h>

#include "lfqueue.h"
static int _lfqueue_enqueue(struct lfqueue *queue, const void *data)
{
    struct lfqueue_item *tail;
    struct lfqueue_item *item = malloc(sizeof(*item));
    if (item == NULL) {
        return -1;
    }
    item->data = data;
    atomic_store(&item->next, (uintptr_t)NULL);
    tail =
        (struct lfqueue_item *)atomic_exchange(&queue->tail, (uintptr_t)item);
    atomic_store(&tail->next, (uintptr_t)item);
    return 0;
}

static void _lfqueue_fini(struct lfqueue *queue)
{
    struct lfqueue_item *it = (void *)atomic_load(&queue->head.next), *next;
    while (it && it->next != (uintptr_t)&queue->head) {
        next = (void *)atomic_load(&it->next);
        free(it);
        it = next;
    }
    free(queue);
}

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
        fn((void *)it->data, carry);
        free(it);
        it = next;
    }
}

static struct lfqueue_ops _lfqueue_ops = {.fini = &_lfqueue_fini,
                                          .enqueue = &_lfqueue_enqueue,
                                          .poll = &_lfqueue_poll};

int lfqueue_init(struct lfqueue **queue)
{
    *queue = malloc(sizeof(**queue));
    if (*queue == NULL) {
        return -1;
    }
    memset(*queue, 0, sizeof(**queue));
    atomic_store(&(*queue)->head.next, (uintptr_t) & (*queue)->head);
    (*queue)->tail = (uintptr_t) & (*queue)->head;
    (*queue)->ops = &_lfqueue_ops;
    return 0;
}
