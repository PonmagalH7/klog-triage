#include "queue.h"
#include <string.h>
#include <time.h>

#define AGING_INTERVAL_SECONDS 5

int effective_priority(const KernelEvent *event, time_t now)
{
    long waited = (long)difftime(now, event->arrival_time);

    int promotion = (int)(waited / AGING_INTERVAL_SECONDS);
    int priority = (int)event->level - promotion;

    if (priority < 0)
        priority = 0;

    return priority;
}

void queue_init(PriorityQueue *q)
{
    q->count = 0;
    q->shutdown = 0;

    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void queue_destroy(PriorityQueue *q)
{
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

int queue_push(PriorityQueue *q, KernelEvent event)
{
    pthread_mutex_lock(&q->mutex);

    if (q->shutdown)
    {
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }

    while (q->count == QUEUE_SIZE && !q->shutdown)
        pthread_cond_wait(&q->not_full, &q->mutex);

    if (q->shutdown)
    {
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }

    event.arrival_time = time(NULL);

    q->events[q->count] = event;
    q->count++;

    pthread_cond_signal(&q->not_empty);

    pthread_mutex_unlock(&q->mutex);

    return 0;
}

int queue_pop(PriorityQueue *q, KernelEvent *event)
{
    pthread_mutex_lock(&q->mutex);

    while (q->count == 0 && !q->shutdown)
        pthread_cond_wait(&q->not_empty, &q->mutex);

    if (q->count == 0 && q->shutdown)
    {
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }

    time_t now = time(NULL);
    int best = 0;

    for (int i = 1; i < q->count; i++)
    {
        int current_priority =
            effective_priority(&q->events[i], now);

        int best_priority =
            effective_priority(&q->events[best], now);

        if (current_priority < best_priority ||
            (current_priority == best_priority &&
             q->events[i].arrival_time <
                 q->events[best].arrival_time))
        {
            best = i;
        }
    }

    *event = q->events[best];

    for (int i = best + 1; i < q->count; i++)
        q->events[i - 1] = q->events[i];

    q->count--;

    pthread_cond_signal(&q->not_full);

    pthread_mutex_unlock(&q->mutex);

    return 0;
}

void queue_shutdown(PriorityQueue *q)
{
    pthread_mutex_lock(&q->mutex);

    q->shutdown = 1;

    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);

    pthread_mutex_unlock(&q->mutex);
}