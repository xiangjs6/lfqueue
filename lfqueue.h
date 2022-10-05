#ifndef LFQUEUE_H
#define LFQUEUE_H

struct lfqueue;
struct lfqueue_ops {
    void (*fini)(struct lfqueue *);
    int (*enqueue)(struct lfqueue *, const void *data);
    int (*dequeue)(struct lfqueue *, void **data);
    void (*poll)(struct lfqueue *, void (*)(void *, void *), void *carry);
    struct lfqueue_item *(*kick)(struct lfqueue *);
    void *(*next)(struct lfqueue *, struct lfqueue_item **);
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

#endif // LFQUEUE_H
