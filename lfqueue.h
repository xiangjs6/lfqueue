#ifndef LFQUEUE_H
#define LFQUEUE_H

struct lfqueue_item;
struct lfqueue;
struct lfqueue_ops {
    void (*fini)(struct lfqueue *);
    int (*enqueue)(struct lfqueue *, const void *data);
    int (*dequeue)(struct lfqueue *, void **data);
    void (*poll)(struct lfqueue *, void (*)(void *, void *), void *carry);
    void (*kick)(struct lfqueue *, struct lfqueue_item *);
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

#define LFQUEUE_KICK_PUSH(p, n)                                                \
    do {                                                                       \
        (p)->next = (uintptr_t)(n);                                            \
        (n)->next = (uintptr_t)NULL;                                           \
    } while (0)

#endif // LFQUEUE_H
