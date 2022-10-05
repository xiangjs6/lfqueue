#include <stdatomic.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>

#include "lfqueue.h"

#define PRODUCER_THREAD_NUMBER 6
#define CONSUMER_THREAD_NUMBER 2
#define ADD_NUMBER (100 * 1024 * 1024)
#define TOTAL_NUMBER ((unsigned long)ADD_NUMBER * PRODUCER_THREAD_NUMBER)

static pthread_t start_thread(int (*f)(void *), void *p)
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
    struct lfqueue_item entry;
    unsigned long long number;
};

atomic_ullong acnt = 0;

int enque_fn(void *args)
{
    struct lfqueue *lf_queue = ((void **)args)[0];
    // memory leak
    struct queue_data *array = calloc(ADD_NUMBER, sizeof(*array));
    for (long long n = 0; n < ADD_NUMBER; ++n) {
        array[n].number = atomic_fetch_add(&acnt, 1);
        if (lf_queue->ops->enqueue(lf_queue, &array[n]) != 0) {
            printf("enqueue error\n");
            return -1;
        }
    }
    return 0;
}

void iter_fn(void *data, void *carry)
{
    struct queue_data *d = data;
    atomic_bool *table_test = carry;
    bool b = atomic_load(&table_test[d->number]);
    if (b == true) {
        printf("%llu dup\n", d->number);
    }
    atomic_store(&table_test[d->number], true);
}

int deque_fn(void *args)
{
    struct lfqueue *lf_queue = ((void **)args)[0];
    void *data;
    while (acnt != TOTAL_NUMBER) {
        if (lf_queue->ops->dequeue(lf_queue, &data) == 0) {
            iter_fn(data, ((void **)args)[1]);
        }
    }
    return 0;
}

int kick_fn(void *args)
{
    struct lfqueue *lf_queue = ((void **)args)[0];
    struct lfqueue_item *item;
    void *data;
    while (acnt != TOTAL_NUMBER) {
        item = lf_queue->ops->kick(lf_queue);
        while ((data = lf_queue->ops->next(lf_queue, &item)) != NULL) {
            iter_fn(data, ((void **)args)[1]);
        }
    }
    return 0;
}

int poll_fn(void *args)
{
    struct lfqueue *lf_queue = ((void **)args)[0];
    while (acnt != TOTAL_NUMBER) {
        lf_queue->ops->poll(lf_queue, &iter_fn, ((void **)args)[1]);
    }
    return 0;
}

void test_lfqueue(atomic_bool *table_test)
{
    acnt = 0;
    pthread_t p_pid_list[PRODUCER_THREAD_NUMBER];
    pthread_t c_pid_list[CONSUMER_THREAD_NUMBER];
    struct lfqueue *queue;
    void **args = malloc(sizeof(void *) * 2);
    lfqueue_init(&queue, offsetof(struct queue_data, entry));
    args[0] = queue;
    args[1] = table_test;
    for (size_t i = 0; i < CONSUMER_THREAD_NUMBER; i++) {
        c_pid_list[i] = start_thread(&kick_fn, args);
    }
    for (size_t i = 0; i < PRODUCER_THREAD_NUMBER; i++) {
        p_pid_list[i] = start_thread(&enque_fn, args);
    }

    for (size_t i = 0; i < PRODUCER_THREAD_NUMBER; i++) {
        pthread_join(p_pid_list[i], NULL);
    }
    for (size_t i = 0; i < CONSUMER_THREAD_NUMBER; i++) {
        pthread_join(c_pid_list[i], NULL);
    }
    queue->ops->poll(queue, iter_fn, table_test);
    queue->ops->fini(queue);
    free(args);
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
    atomic_bool *table_test = malloc(TOTAL_NUMBER * sizeof(*table_test));
    printf("Number of producer threads: %d\n", PRODUCER_THREAD_NUMBER);
    printf("Number of consumer threads: %d\n", CONSUMER_THREAD_NUMBER);

    memset(table_test, 0, TOTAL_NUMBER * sizeof(*table_test));
    gettimeofday(&start_tv, 0);
    test_lfqueue(table_test);
    gettimeofday(&stop_tv, 0);
    for (size_t i = 0; i < TOTAL_NUMBER; i++) {
        if (table_test[i] == false) {
            printf("%lu error no atomic\n", i);
        }
    }
    printf("lock free queue cost time %lfs ops: %lf/s\n",
           timeval_diff(&stop_tv, &start_tv),
           (double)TOTAL_NUMBER / timeval_diff(&stop_tv, &start_tv));
    return 0;
}
