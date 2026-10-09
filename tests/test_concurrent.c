
#include "../src/queue.h"
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>

#define PRODUCER_COUNT 3
#define WORKER_COUNT 3
#define EVENTS_PER_PRODUCER 100
#define TOTAL_EVENTS (PRODUCER_COUNT * EVENTS_PER_PRODUCER)

PriorityQueue queue;
atomic_int processed = 0;
atomic_int failed = 0;
atomic_int seen[TOTAL_EVENTS] = {0};

void *producer_function(void *arg)
{
    int producer_id = *(int *)arg;

    for (int i = 0; i < EVENTS_PER_PRODUCER; i++)
    {
        KernelEvent event = {
            .level = (unsigned)(i % 5),
            .seq = (unsigned long long)(producer_id * EVENTS_PER_PRODUCER + i),
            .ts_us = 0,
            .message = "Concurrent test event"};

        if (queue_push(&queue, event) != 0)
            atomic_fetch_add(&failed, 1);
    }

    return NULL;
}

void *worker_function(void *arg)
{
    (void)arg;

    KernelEvent event;

    while (queue_pop(&queue, &event) == 0)
    {
        if (event.seq < TOTAL_EVENTS)
        {
            atomic_fetch_add(&seen[event.seq], 1);
        }

        atomic_fetch_add(&processed, 1);
    }

    return NULL;
}

int main(void)
{
    pthread_t producers[PRODUCER_COUNT];
    pthread_t workers[WORKER_COUNT];
    int producer_ids[PRODUCER_COUNT];

    queue_init(&queue);

    for (int i = 0; i < WORKER_COUNT; i++)
        pthread_create(&workers[i], NULL, worker_function, NULL);

    for (int i = 0; i < PRODUCER_COUNT; i++)
    {
        producer_ids[i] = i;
        pthread_create(&producers[i], NULL,
                       producer_function, &producer_ids[i]);
    }

    for (int i = 0; i < PRODUCER_COUNT; i++)
        pthread_join(producers[i], NULL);

    queue_shutdown(&queue);

    for (int i = 0; i < WORKER_COUNT; i++)
        pthread_join(workers[i], NULL);

    printf("Expected events: %d\n", TOTAL_EVENTS);
    printf("Processed events: %d\n", atomic_load(&processed));
    printf("Failed insertions: %d\n", atomic_load(&failed));

    int all_once = 1;

    for (int i = 0; i < TOTAL_EVENTS; i++)
    {
        if (atomic_load(&seen[i]) != 1)
        {
            all_once = 0;
            printf("Event ID %d processed %d times\n",
                   i, atomic_load(&seen[i]));
        }
    }

    if (atomic_load(&processed) == TOTAL_EVENTS &&
        atomic_load(&failed) == 0 && all_once)

    {
        printf("Concurrent test PASSED.\n");
    }
    else
    {
        printf("Concurrent test FAILED.\n");
    }

    queue_destroy(&queue);

    return 0;
}
