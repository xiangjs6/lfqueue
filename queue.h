#ifndef QUEUE_H
#define QUEUE_H

#define LOCK_CTX pthread_spinlock_t

struct queue;
struct queue_ops {
    void (*fini)(struct queue *);
    int (*enqueue)(struct queue *, const void *data);
    void (*poll)(struct queue *, void (*)(void *, void *), void *carry);
};

struct queue_item {
    const void *data;
    struct queue_item *next;
};

struct queue {
    struct queue_ops *ops;
    struct queue_item head;
    struct queue_item *tail;
    LOCK_CTX lck;
};
int queue_init(struct queue **queue);
#endif // QUEUE_H
