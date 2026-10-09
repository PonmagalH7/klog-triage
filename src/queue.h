#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <time.h>

#define QUEUE_SIZE 100

typedef struct {
    unsigned level;
    unsigned long long seq;
    unsigned long long ts_us;
    time_t arrival_time;
    char message[8192];
} KernelEvent;

typedef struct {
    KernelEvent events[QUEUE_SIZE];
    int count;
    int shutdown;

    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} PriorityQueue;

void queue_init(PriorityQueue *q);

void queue_destroy(PriorityQueue *q);

int queue_push(PriorityQueue *q, KernelEvent event);

int queue_pop(PriorityQueue *q, KernelEvent *event);

void queue_shutdown(PriorityQueue *q);

#endif