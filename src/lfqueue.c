#include <stdatomic.h>
#include <stdbool.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <memory.h>

#define _LFQUEUE_SOURCE
#include "lfqueue.h"

#define RINGBUF(q) ((q)->buf)

// lfqueue link
static void _lfqueue_link_enqueue(struct lfqueue *queue, const void *data)
{
    struct lfqueue_link_entry *tail;
    struct lfqueue_link_entry *item = (void *)((char *)data + queue->link._off);
    atomic_store(&item->queue_id, (uintptr_t)&queue->link.head);
    atomic_store(&item->next, (uintptr_t)NULL);
    tail = (struct lfqueue_link_entry *)atomic_exchange(&queue->link.tail,
                                                        (uintptr_t)item);
    atomic_store(&tail->next, (uintptr_t)item);
}

static void *_lfqueue_link_dequeue(struct lfqueue *queue)
{
    struct lfqueue_link_entry *item;
    uintptr_t expected;
    item = (struct lfqueue_link_entry *)atomic_exchange(
        &queue->link.head.next, (uintptr_t)&queue->link.head);
    if (item == &queue->link.head) {
        return NULL;
    }
    expected = (uintptr_t)item;
    if (atomic_compare_exchange_strong(&queue->link.tail, &expected,
                                       (uintptr_t)&queue->link.head) == false) {
        do {
            expected = atomic_load(&item->next);
        } while (expected == (uintptr_t)NULL);
        atomic_store(&queue->link.head.next, expected);
    }
    atomic_store(&item->queue_id, (uintptr_t)NULL);
    return (void *)((char *)item - queue->link._off);
}

static void _lfqueue_link_poll(struct lfqueue *queue,
                               void (*fn)(void *, void *), void *carry)
{
    struct lfqueue_link_entry *item, *next, *tail;
    item = (struct lfqueue_link_entry *)atomic_exchange(
        &queue->link.head.next, (uintptr_t)&queue->link.head);
    if (item == &queue->link.head) {
        return;
    }

    tail = (struct lfqueue_link_entry *)atomic_exchange(
        &queue->link.tail, (uintptr_t)&queue->link.head);
    atomic_store(&tail->next, (uintptr_t)&queue->link.head);
    while (item != &queue->link.head) {
        do {
            next = (void *)atomic_load(&item->next);
        } while (next == NULL);
        atomic_store(&item->queue_id, (uintptr_t)NULL);
        fn((void *)((char *)item - queue->link._off), carry);
        item = next;
    }
}

static void _lfqueue_link_kick(struct lfqueue *queue,
                               struct lfqueue_link_entry *tail)
{
    struct lfqueue_link_entry *head = (struct lfqueue_link_entry *)tail->next;
    atomic_store(&tail->next, (uintptr_t)NULL);
    tail = (struct lfqueue_link_entry *)atomic_exchange(&queue->link.tail,
                                                        (uintptr_t)tail);
    atomic_store(&tail->next, (uintptr_t)head);
}

static struct lfqueue_link_entry *_lfqueue_link_fetch(struct lfqueue *queue)
{
    struct lfqueue_link_entry *head, *tail;
    head = (struct lfqueue_link_entry *)atomic_exchange(
        &queue->link.head.next, (uintptr_t)&queue->link.head);
    if (head == &queue->link.head) {
        return NULL;
    }
    tail = (struct lfqueue_link_entry *)atomic_exchange(
        &queue->link.tail, (uintptr_t)&queue->link.head);
    atomic_store(&tail->next, (uintptr_t)head);
    return tail;
}

static bool _lfqueue_link_empty(struct lfqueue *queue)
{
    return atomic_load(&queue->link.head.next) == (uintptr_t)&queue->link.head;
}

static bool _lfqueue_link_inside(struct lfqueue *queue, const void *data)
{
    struct lfqueue_link_entry *item = (void *)((char *)data + queue->link._off);
    return atomic_load(&item->queue_id) == (uintptr_t)&queue->link.head;
}

// lfqueue ring
static void _lfqueue_ring_enqueue(struct lfqueue *queue, const void *data)
{
    uintptr_t tail = atomic_fetch_add(&queue->ring.tail, 1);
    uintptr_t head = atomic_load(&queue->ring.head);
    assert(tail - head < 2 * queue->ring.size ||
           head - tail < queue->ring.size);
    assert(data != &queue->ring.empty_mark);
    while ((void *)atomic_load(&RINGBUF(queue)[tail % queue->ring.size]) !=
           &queue->ring.empty_mark) {
        continue;
    }
    atomic_store(&RINGBUF(queue)[tail % queue->ring.size], (uintptr_t)data);
}

static void *_lfqueue_ring_dequeue(struct lfqueue *queue)
{
    uintptr_t head = atomic_fetch_add(&queue->ring.head, 1);
    uintptr_t tail = atomic_load(&queue->ring.tail);
    void *data;
    assert(tail - head < 2 * queue->ring.size ||
           head - tail < queue->ring.size);
    while ((data = (void *)atomic_load(&RINGBUF(
                queue)[head % queue->ring.size])) == &queue->ring.empty_mark) {
        continue;
    }
    atomic_store(&RINGBUF(queue)[head % queue->ring.size],
                 (uintptr_t)&queue->ring.empty_mark);
    return data;
}

static void _lfqueue_fini(struct lfqueue *queue) { free(queue); }

static struct lfqueue_ops _lfqueue_link_ops = {
    .fini = &_lfqueue_fini,
    .enqueue = &_lfqueue_link_enqueue,
    .dequeue = &_lfqueue_link_dequeue,
    .poll = &_lfqueue_link_poll,
    .kick = &_lfqueue_link_kick,
    .fetch = &_lfqueue_link_fetch,
    .empty = &_lfqueue_link_empty,
    .inside = &_lfqueue_link_inside};

static struct lfqueue_ops _lfqueue_ring_ops = {
    .fini = &_lfqueue_fini,
    .enqueue = &_lfqueue_ring_enqueue,
    .dequeue = &_lfqueue_ring_dequeue,
    .poll = NULL,
    .kick = NULL,
    .fetch = NULL,
    .empty = NULL,
    .inside = NULL};

struct lfqueue *lfqueue(int method, ...)
{
    va_list va;
    size_t off;
    size_t size;
    void *ptr;
    struct lfqueue *queue = malloc(sizeof(*queue));
    if (queue == NULL) {
        return NULL;
    }
    memset(queue, 0, sizeof(*queue));
    queue->method = method;
    va_start(va, method);
    switch (method) {
        case LF_METHOD_LINK:
            off = va_arg(va, size_t);
            atomic_store(&queue->link.head.next, (uintptr_t)&queue->link.head);
            queue->link._off = off;
            queue->link.tail = (uintptr_t)&queue->link.head;
            queue->ops = &_lfqueue_link_ops;
            break;
        case LF_METHOD_RING:
            size = va_arg(va, size_t);
            if (size == 0 || ((size - 1) & size)) {
                free(queue);
                queue = NULL;
                break;
            }
            queue->ring.size = size;
            ptr = realloc(queue, sizeof(*queue) + sizeof(*queue->buf) * size);
            if (ptr == NULL) {
                free(queue);
                break;
            }
            queue = ptr;
            for (size_t i = 0; i < size; i++) {
                atomic_store(&RINGBUF(queue)[i],
                             (uintptr_t)&queue->ring.empty_mark);
            }
            queue->ops = &_lfqueue_ring_ops;
            break;
        default:
            free(queue);
            va_end(va);
            queue = NULL;
            break;
    }
    va_end(va);
    return queue;
}
