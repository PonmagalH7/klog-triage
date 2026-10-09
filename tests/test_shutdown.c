
#include "../src/queue.h"
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

void *producer_function(void *arg)
{
    PriorityQueue *queue = (PriorityQueue *)arg;

    KernelEvent event = {
        .level = 2,
        .seq = 999,
        .ts_us = 9000,
        .message = "Extra test event"
    };

    printf("Producer: trying to add an event to the full queue...\n");

    int result = queue_push(queue, event);

    if (result == -1)
        printf("Producer: stopped safely during shutdown.\n");
    else
        printf("Producer: event added.\n");

    return NULL;
}

int main(void)
{
    PriorityQueue queue;
    queue_init(&queue);

    KernelEvent event = {
        .level = 4,
        .seq = 1,
        .ts_us = 1000,
        .message = "Queue filling event"
    };

    for (int i = 0; i < QUEUE_SIZE; i++)
    {
        event.seq = i + 1;
        queue_push(&queue, event);
    }

    printf("Queue filled with %d events.\n", QUEUE_SIZE);

    pthread_t producer;
    pthread_create(&producer, NULL, producer_function, &queue);

    sleep(1);

    printf("Starting shutdown...\n");
    queue_shutdown(&queue);

    pthread_join(producer, NULL);

    queue_destroy(&queue);

    printf("Shutdown test completed.\n");

    return 0;
}
