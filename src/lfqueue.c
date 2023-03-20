#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <memory.h>

#define _LFQUEUE_SOURCE
#include "lfqueue.h"

static void _lfqueue_enqueue(struct lfqueue *queue, const void *data)
{
    atomic_uintptr_t *tail;
    struct lfqueue_entry *item = (void *)((char *)data + queue->off);
    unsigned long cur =
        atomic_fetch_add(&queue->pcur_link, 1) % queue->link_count;
    atomic_store(&item->queue_id, (uintptr_t)queue);
    atomic_store(&item->next, (uintptr_t)NULL);
    tail = (atomic_uintptr_t *)atomic_exchange(&queue->link[cur].tail,
                                               (uintptr_t)&item->next);
    atomic_store(tail, (uintptr_t)item);
}

static void *_lfqueue_dequeue(struct lfqueue *queue)
{
    struct lfqueue_entry *item;
    uintptr_t expected;
    unsigned long cur =
        atomic_fetch_add(&queue->ccur_link, 1) % queue->link_count;
    item = (struct lfqueue_entry *)atomic_exchange(
        &queue->link[cur].head, (uintptr_t)&queue->link[cur].head);
    if (item == (void *)&queue->link[cur].head) {
        return NULL;
    }
    expected = (uintptr_t)&item->next;
    if (atomic_compare_exchange_strong(&queue->link[cur].tail, &expected,
                                       (uintptr_t)&queue->link[cur].head) ==
        false) {
        do {
            expected = atomic_load(&item->next);
        } while (expected == (uintptr_t)NULL);
        atomic_store(&queue->link[cur].head, expected);
    }
    atomic_store(&item->queue_id, (uintptr_t)NULL);
    return (void *)((char *)item - queue->off);
}

static void _lfqueue_poll(struct lfqueue *queue, void (*fn)(void *, void *),
                          void *carry)
{
    struct lfqueue_entry *item, *next;
    uintptr_t *tail;
    unsigned long cur =
        atomic_fetch_add(&queue->ccur_link, 1) % queue->link_count;
    item = (struct lfqueue_entry *)atomic_exchange(
        &queue->link[cur].head, (uintptr_t)&queue->link[cur].head);
    if (item == (void *)&queue->link[cur].head) {
        return;
    }

    tail = (uintptr_t *)atomic_exchange(&queue->link[cur].tail,
                                        (uintptr_t)&queue->link[cur].head);
    *tail = (uintptr_t)&queue->link[cur].head;
    while (item != (void *)&queue->link[cur].head) {
        do {
            next = (void *)atomic_load(&item->next);
        } while (next == NULL);
        atomic_store(&item->queue_id, (uintptr_t)NULL);
        fn((void *)((char *)item - queue->off), carry);
        item = next;
    }
}

static void _lfqueue_kick(struct lfqueue *queue, struct lfqueue_entry *node)
{
    atomic_uintptr_t *tail;
    struct lfqueue_entry *head = (struct lfqueue_entry *)node->next;
    unsigned long cur =
        atomic_fetch_add(&queue->pcur_link, 1) % queue->link_count;
    atomic_store(&node->next, (uintptr_t)NULL);
    tail = (atomic_uintptr_t *)atomic_exchange(&queue->link[cur].tail,
                                               (uintptr_t)&node->next);
    atomic_store(tail, (uintptr_t)head);
}

static struct lfqueue_entry *_lfqueue_fetch(struct lfqueue *queue)
{
    struct lfqueue_entry *head;
    uintptr_t *tail;
    unsigned long cur =
        atomic_fetch_add(&queue->ccur_link, 1) % queue->link_count;
    head = (struct lfqueue_entry *)atomic_exchange(
        &queue->link[cur].head, (uintptr_t)&queue->link[cur].head);
    if (head == (void *)&queue->link[cur].head) {
        return NULL;
    }
    tail = (uintptr_t *)atomic_exchange(&queue->link[cur].tail,
                                        (uintptr_t)&queue->link[cur].head);
    *tail = (uintptr_t)head;
    return (struct lfqueue_entry *)((char *)tail -
                                    offsetof(struct lfqueue_entry, next));
}

static bool _lfqueue_inside(struct lfqueue *queue, const void *data)
{
    struct lfqueue_entry *item = (void *)((char *)data + queue->off);
    return atomic_load(&item->queue_id) == (uintptr_t)queue;
}

static bool _lfqueue_empty(struct lfqueue *queue)
{
    for (size_t i = 0; i < queue->link_count; i++) {
        if (atomic_load(&queue->link[i].head) !=
            (uintptr_t)&queue->link[i].head) {
            return false;
        }
    }
    return true;
}

static void _lfqueue_fini(struct lfqueue *queue) { free(queue); }

static struct lfqueue_ops _lfqueue_link_ops = {.fini = &_lfqueue_fini,
                                               .enqueue = &_lfqueue_enqueue,
                                               .dequeue = &_lfqueue_dequeue,
                                               .poll = &_lfqueue_poll,
                                               .kick = &_lfqueue_kick,
                                               .fetch = &_lfqueue_fetch,
                                               .inside = &_lfqueue_inside,
                                               .empty = &_lfqueue_empty};

struct lfqueue *lfqueue(int concurrent, size_t off)
{
    struct lfqueue *queue;
    if (concurrent <= 0 || ((concurrent - 1) & concurrent)) {
        return NULL;
    }
    queue = malloc(sizeof(*queue) + sizeof(*queue->link) * concurrent);
    if (queue == NULL) {
        return NULL;
    }
    memset(queue, 0, sizeof(*queue) + sizeof(*queue->link) * concurrent);
    for (size_t i = 0; i < (size_t)concurrent; i++) {
        atomic_store(&queue->link[i].head, (uintptr_t)&queue->link[i].head);
        atomic_store(&queue->link[i].tail, (uintptr_t)&queue->link[i].head);
    }
    queue->off = off;
    queue->link_count = concurrent;
    queue->ops = &_lfqueue_link_ops;
    return queue;
}
