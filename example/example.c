#include <stdatomic.h>
#include <pthread.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>

#include "lfqueue.h"

#define PRODUCER_THREAD_NUMBER 8
#define CONSUMER_THREAD_NUMBER 8
#define ADD_NUMBER (100 * 1024 * 1024)
#define TOTAL_NUMBER ((unsigned long)ADD_NUMBER * PRODUCER_THREAD_NUMBER)
#define KICK_BATCH 100

static pthread_t start_thread(void *(*f)(void *), void *p)
{
    pthread_t thread_id = (pthread_t)0;
    /*pthread_attr_t attr;*/
    /*(void)pthread_attr_init(&attr);*/
    /*(void)pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);*/
    pthread_create(&thread_id, NULL, (void *(*)(void *))f, p);
    /*pthread_attr_destroy(&attr);*/
    return thread_id;
}

struct queue_data {
    struct lfqueue_entry entry;
    unsigned long long number;
};

atomic_ullong pcnt = 0;
atomic_ullong ccnt = 0;

void *enque_fn(void *args)
{
    struct lfqueue *lf_queue = ((void **)args)[0];
    // memory leak
    struct queue_data *array = ((void **)args)[2];
    for (long long n = 0; n < ADD_NUMBER; ++n) {
        array[n].number = atomic_fetch_add(&pcnt, 1);
        lf_queue->ops->enqueue(lf_queue, &array[n]);
    }
    free(args);
    return 0;
}

void *kick_fn(void *args)
{
    struct lfqueue *lf_queue = ((void **)args)[0];
    struct lfqueue_entry *tail = NULL;
    // memory leak
    struct queue_data *array = ((void **)args)[2];
    for (long long n = 0; n < ADD_NUMBER; ++n) {
        array[n].number = atomic_fetch_add(&pcnt, 1);
        if (n % KICK_BATCH == 0) {
            LFQUEUE_SUBQ_INIT(lf_queue, &array[n].entry, tail);
        } else {
            LFQUEUE_SUBQ_PUSH(tail, &array[n].entry);
        }
        if ((n + 1) % KICK_BATCH == 0) {
            lf_queue->ops->kick(lf_queue, tail);
        }
    }
    if (ADD_NUMBER % KICK_BATCH != 0) {
        lf_queue->ops->kick(lf_queue, tail);
    }
    free(args);
    return 0;
}

void iter_link_fn(void *data, void *carry)
{
    struct queue_data *d = data;
    atomic_bool *table_test = carry;
    bool b = atomic_load(&table_test[d->number]);
    if (b == true) {
        printf("%llu dup\n", d->number);
    }
    atomic_store(&table_test[d->number], true);
    atomic_fetch_add(&ccnt, 1);
}

void *deque_fn(void *args)
{
    struct lfqueue *lf_queue = ((void **)args)[0];
    void *data;
    while (atomic_load(&ccnt) != TOTAL_NUMBER) {
        if ((data = lf_queue->ops->dequeue(lf_queue)) != NULL) {
            iter_link_fn(data, ((void **)args)[1]);
        }
    }
    free(args);
    return 0;
}

void *fetch_pop_fn(void *args)
{
    struct lfqueue *lf_queue = ((void **)args)[0];
    struct lfqueue_entry *tail, *value;
    void *data;
    while (atomic_load(&ccnt) != TOTAL_NUMBER) {
        tail = lf_queue->ops->fetch(lf_queue);
        while (tail) {
            LFQUEUE_SUBQ_POP(tail, value);
            data = (char *)value - offsetof(struct queue_data, entry);
            iter_link_fn(data, ((void **)args)[1]);
        }
    }
    free(args);
    return 0;
}

void *poll_fn(void *args)
{
    struct lfqueue *lf_queue = ((void **)args)[0];
    while (atomic_load(&ccnt) != TOTAL_NUMBER) {
        lf_queue->ops->poll(lf_queue, &iter_link_fn, ((void **)args)[1]);
    }
    free(args);
    return 0;
}

void test_lfqueue(struct lfqueue *queue, atomic_bool *table_test,
                  struct queue_data *array)
{
    pthread_t p_pid_list[PRODUCER_THREAD_NUMBER];
    pthread_t c_pid_list[CONSUMER_THREAD_NUMBER];
    void **args;
    for (size_t i = 0; i < PRODUCER_THREAD_NUMBER; i++) {
        args = malloc(sizeof(*args) * 3);
        args[0] = queue;
        args[1] = table_test;
        args[2] = &array[i * ADD_NUMBER];
        p_pid_list[i] = start_thread(&kick_fn, args);
    }
    for (size_t i = 0; i < CONSUMER_THREAD_NUMBER; i++) {
        args = malloc(sizeof(*args) * 2);
        args[0] = queue;
        args[1] = table_test;
        c_pid_list[i] = start_thread(&poll_fn, args);
    }

    for (size_t i = 0; i < PRODUCER_THREAD_NUMBER; i++) {
        pthread_join(p_pid_list[i], NULL);
    }
    for (size_t i = 0; i < CONSUMER_THREAD_NUMBER; i++) {
        pthread_join(c_pid_list[i], NULL);
    }
}

double timeval_diff(struct timeval *tv0, struct timeval *tv1)
{
    double time1, time2;

    time1 = tv0->tv_sec + (tv0->tv_usec / 1000000.0);
    time2 = tv1->tv_sec + (tv1->tv_usec / 1000000.0);

    time1 = time1 - time2;
    if (time1 < 0)
        time1 = -time1;
    return time1;
}

int main(void)
{
    struct timeval start_tv, stop_tv;
    atomic_bool *table_test;
    struct queue_data *array;
    struct lfqueue *queue;
    printf("Number of producer threads: %d\n", PRODUCER_THREAD_NUMBER);
    printf("Number of consumer threads: %d\n", CONSUMER_THREAD_NUMBER);
    table_test = malloc(TOTAL_NUMBER * sizeof(*table_test));
    if (table_test == NULL) {
        return -1;
    }
    array = malloc(TOTAL_NUMBER * sizeof(*array));
    if (array == NULL) {
        free(table_test);
        return -1;
    }

    for (size_t i = 0; i < TOTAL_NUMBER; i++) {
        array[i].number = i;
        array[i].entry.next = i;
        array[i].entry.queue_id = i;
        table_test[i] = i;
    }
    memset(table_test, 0, TOTAL_NUMBER * sizeof(*table_test));
    memset(array, 0, TOTAL_NUMBER * sizeof(*array));

    queue = lfqueue(1 * (PRODUCER_THREAD_NUMBER > CONSUMER_THREAD_NUMBER
                             ? PRODUCER_THREAD_NUMBER
                             : CONSUMER_THREAD_NUMBER),
                    offsetof(struct queue_data, entry));
    assert(queue != NULL);
    gettimeofday(&start_tv, 0);
    test_lfqueue(queue, table_test, array);
    gettimeofday(&stop_tv, 0);
    queue->ops->fini(queue);
    for (size_t i = 0; i < TOTAL_NUMBER; i++) {
        if (table_test[i] == false) {
            printf("%lu error no atomic\n", i);
        }
    }
    free(table_test);
    free(array);
    printf("lock free queue cost time %lfs ops: %lf/s\n",
           timeval_diff(&stop_tv, &start_tv),
           (double)TOTAL_NUMBER / timeval_diff(&stop_tv, &start_tv));
    return 0;
}
