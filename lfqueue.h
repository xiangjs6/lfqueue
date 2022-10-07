#ifndef LFQUEUE_H
#define LFQUEUE_H

struct lfqueue_item;
struct lfqueue;
struct lfqueue_ops {
    void (*fini)(struct lfqueue *);
    void (*enqueue)(struct lfqueue *, const void *data);
    void *(*dequeue)(struct lfqueue *);
    void (*poll)(struct lfqueue *, void (*)(void *, void *), void *carry);
    void (*kick)(struct lfqueue *, struct lfqueue_item *);
    struct lfqueue_item *(*fetch)(struct lfqueue *);
    bool (*empty)(struct lfqueue *);
    bool (*inside)(struct lfqueue *, const void *data);
};

struct lfqueue_item {
    volatile atomic_uintptr_t next;
    volatile atomic_uintptr_t queue_id;
};

struct lfqueue {
    struct lfqueue_ops *ops;
    struct lfqueue_item head;
    volatile atomic_uintptr_t tail;
    size_t _off;
};
int lfqueue_init(struct lfqueue **queue, size_t off);

#define LFQUEUE_SUBQ_INIT(q, h, t)                                             \
    do {                                                                       \
        (h)->next = (uintptr_t)(h);                                            \
        (h)->queue_id = (uintptr_t) & (q)->head;                               \
        (t) = (h);                                                             \
    } while (0)

#define LFQUEUE_SUBQ_PUSH(t, n)                                                \
    do {                                                                       \
        (n)->next = (t)->next;                                                 \
        (t)->next = (uintptr_t)(n);                                            \
        (n)->queue_id = (t)->queue_id;                                         \
        (t) = (n);                                                             \
    } while (0)

#define LFQUEUE_SUBQ_POP(t, v)                                                 \
    do {                                                                       \
        while (((struct lfqueue_item *)(t)->next)->next == (uintptr_t)NULL)    \
            ;                                                                  \
        (v) = (struct lfqueue_item *)(t)->next;                                \
        if ((v) == (t)) {                                                      \
            (t) = NULL;                                                        \
        } else {                                                               \
            (t)->next = ((struct lfqueue_item *)(t)->next)->next;              \
        }                                                                      \
        (v)->queue_id = (uintptr_t)NULL;                                       \
    } while (0)

#endif // LFQUEUE_H
