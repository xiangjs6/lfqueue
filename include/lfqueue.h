#ifndef LFQUEUE_H
#define LFQUEUE_H

struct lfqueue_entry {
    volatile atomic_uintptr_t next;
    volatile atomic_uintptr_t queue_id;
};

struct lfqueue {
#ifdef _LFQUEUE_SOURCE
    const struct lfqueue_ops {
#else
    const struct {
#endif
        void (*fini)(struct lfqueue *);
        void (*enqueue)(struct lfqueue *, const void *data);
        void *(*dequeue)(struct lfqueue *);
        void (*poll)(struct lfqueue *, void (*)(void *, void *), void *carry);
        void (*kick)(struct lfqueue *, struct lfqueue_entry *);
        struct lfqueue_entry *(*fetch)(struct lfqueue *);
        bool (*inside)(struct lfqueue *, const void *data);
    } *ops;
#ifdef _LFQUEUE_SOURCE
    size_t off;
    size_t link_count;
    atomic_ulong ccur_link;
    atomic_ulong pcur_link;
    struct {
        volatile atomic_uintptr_t head;
        volatile atomic_uintptr_t tail;
    } link[];
#endif
};

struct lfqueue *lfqueue(int concurrent, size_t off);

#define LFQUEUE_SUBQ_INIT(q, h, t)                                             \
    do {                                                                       \
        (h)->next = (uintptr_t)(h);                                            \
        (h)->queue_id = (uintptr_t)(q);                                        \
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
        while (((struct lfqueue_entry *)(t)->next) == NULL ||                  \
               ((struct lfqueue_entry *)(t)->next)->next == (uintptr_t)NULL)   \
            ;                                                                  \
        (v) = (struct lfqueue_entry *)(t)->next;                               \
        if ((v) == (t)) {                                                      \
            (t) = NULL;                                                        \
        } else {                                                               \
            (t)->next = ((struct lfqueue_entry *)(t)->next)->next;             \
        }                                                                      \
        (v)->queue_id = (uintptr_t)NULL;                                       \
    } while (0)

#endif // LFQUEUE_H
