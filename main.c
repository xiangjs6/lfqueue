#include <stdatomic.h>
#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>

#include "lfqueue.h"
#include "queue.h"

#define PRODUCER_THREAD_NUMBER 16
#define CONSUMER_THREAD_NUMBER 4
#define ADD_NUMBER (1024 * 1024)
#define TOTAL_NUMBER ((unsigned long)ADD_NUMBER * PRODUCER_THREAD_NUMBER)

#define TYPE_QUEUE 1
#define TYPE_LF_QUEUE 2

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

atomic_ullong acnt = 0;

int fn(void *args)
{
    struct lfqueue *queue = args;
    for (int n = 0; n < ADD_NUMBER; ++n) {
        long cnt = atomic_fetch_add(&acnt, 1);
        if (queue->ops->enqueue(queue, (void *)cnt) != 0) {
            printf("enqueue error\n");
            return -1;
        }
    }
    return 0;
}

void iter_fn(void *data, void *carry)
{
    atomic_bool *table_test = carry;
    if (table_test[(long)data] == true) {
        printf("%ld dup\n", (long)data);
    }
    table_test[(long)data] = true;
}

int poll_fn(void *args)
{
    long type = (long)((void **)args)[0];
    struct lfqueue *lf_queue = ((void **)args)[1];
    struct queue *queue = ((void **)args)[1];
    if (type == TYPE_QUEUE) {
        while (acnt != TOTAL_NUMBER) {
            queue->ops->poll(queue, &iter_fn, ((void **)args)[2]);
        }
    } else if (type == TYPE_LF_QUEUE) {
        while (acnt != TOTAL_NUMBER) {
            lf_queue->ops->poll(lf_queue, &iter_fn, ((void **)args)[2]);
        }
    }
    return 0;
}

void test_lfqueue(atomic_bool *table_test)
{
    acnt = 0;
    pthread_t p_pid_list[PRODUCER_THREAD_NUMBER];
    pthread_t c_pid_list[CONSUMER_THREAD_NUMBER];
    struct lfqueue *queue;
    void **args = malloc(sizeof(void *) * 3);
    lfqueue_init(&queue);
    args[0] = (void *)TYPE_LF_QUEUE;
    args[1] = queue;
    args[2] = table_test;
    for (size_t i = 0; i < PRODUCER_THREAD_NUMBER; i++) {
        p_pid_list[i] = start_thread(&fn, queue);
    }
    for (size_t i = 0; i < CONSUMER_THREAD_NUMBER; i++) {
        c_pid_list[i] = start_thread(&poll_fn, args);
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

void test_queue(atomic_bool *table_test)
{
    acnt = 0;
    pthread_t pid_list[PRODUCER_THREAD_NUMBER];
    pthread_t c_pid_list[CONSUMER_THREAD_NUMBER];
    struct queue *queue;
    void **args = malloc(sizeof(void *) * 3);
    queue_init(&queue);
    args[0] = (void *)TYPE_LF_QUEUE;
    args[1] = queue;
    args[2] = table_test;
    for (size_t i = 0; i < PRODUCER_THREAD_NUMBER; i++) {
        pid_list[i] = start_thread(&fn, queue);
    }
    for (size_t i = 0; i < CONSUMER_THREAD_NUMBER; i++) {
        c_pid_list[i] = start_thread(&poll_fn, args);
    }
    for (size_t i = 0; i < PRODUCER_THREAD_NUMBER; i++) {
        pthread_join(pid_list[i], NULL);
    }
    for (size_t i = 0; i < CONSUMER_THREAD_NUMBER; i++) {
        c_pid_list[i] = start_thread(&poll_fn, args);
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

    memset(table_test, 0, TOTAL_NUMBER * sizeof(*table_test));
    gettimeofday(&start_tv, 0);
    test_queue(table_test);
    gettimeofday(&stop_tv, 0);
    for (size_t i = 0; i < TOTAL_NUMBER; i++) {
        if (table_test[i] == false) {
            printf("%lu error no atomic\n", i);
        }
    }
    printf("lock queue cost time %lfs ops: %lf/s\n",
           timeval_diff(&stop_tv, &start_tv),
           (double)TOTAL_NUMBER / timeval_diff(&stop_tv, &start_tv));
    free(table_test);
    return 0;
}
