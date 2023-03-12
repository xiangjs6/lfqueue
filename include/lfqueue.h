#ifndef LFQUEUE_H
#define LFQUEUE_H

struct lfqueue_link_entry {
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
        void (*kick)(struct lfqueue *, struct lfqueue_link_entry *);
        struct lfqueue_link_entry *(*fetch)(struct lfqueue *);
        bool (*empty)(struct lfqueue *);
        bool (*inside)(struct lfqueue *, const void *data);
    } *ops;
    int method;
    union {
        struct {
            struct lfqueue_link_entry head;
            volatile atomic_uintptr_t tail;
            size_t _off;
        } link;
        struct {
            bool empty_mark;
            atomic_uintptr_t head;
            atomic_uintptr_t tail;
            size_t size;
        } ring;
    };
    atomic_uintptr_t buf[];
};

enum {
    LF_METHOD_LINK = 1,
    LF_METHOD_RING = 2,
};

struct lfqueue *lfqueue(int method, ...);

#define LFQUEUE_SUBQ_INIT(q, h, t)                                             \
    do {                                                                       \
        (h)->next = (uintptr_t)(h);                                            \
        (h)->queue_id = (uintptr_t) & (q)->link.head;                          \
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
        while (((struct lfqueue_link_entry *)(t)->next) == NULL ||             \
               ((struct lfqueue_link_entry *)(t)->next)->next ==               \
                   (uintptr_t)NULL)                                            \
            ;                                                                  \
        (v) = (struct lfqueue_link_entry *)(t)->next;                          \
        if ((v) == (t)) {                                                      \
            (t) = NULL;                                                        \
        } else {                                                               \
            (t)->next = ((struct lfqueue_link_entry *)(t)->next)->next;        \
        }                                                                      \
        (v)->queue_id = (uintptr_t)NULL;                                       \
    } while (0)

#endif // LFQUEUE_H
