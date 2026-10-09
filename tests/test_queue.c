#include "../src/queue.h"
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#define WORKER_COUNT 3
#define TEST_EVENTS 6

void *worker_function(void *arg);

int main(void)
{
    PriorityQueue queue;

    queue_init(&queue);

    pthread_t workers[WORKER_COUNT];

    KernelEvent events[TEST_EVENTS] = {
        {.level = 4, .seq = 101, .ts_us = 1000, .message = "Warning: test warning"},

        {.level = 2, .seq = 102, .ts_us = 2000, .message = "Critical: test critical error"},

        {.level = 3, .seq = 103, .ts_us = 3000, .message = "Error: test error"},

        {.level = 1, .seq = 104, .ts_us = 4000, .message = "Alert: test alert"},

        {.level = 4, .seq = 105, .ts_us = 5000, .message = "Warning: another warning"},

        {.level = 2, .seq = 106, .ts_us = 6000, .message = "Critical: another critical error"}};

    printf("Adding fake kernel events...\n");

    for (int i = 0; i < TEST_EVENTS; i++)
    {
        queue_push(&queue, events[i]);
    }

    printf("Starting %d workers...\n", WORKER_COUNT);

    for (int i = 0; i < WORKER_COUNT; i++)
    {
        pthread_create(&workers[i], NULL, worker_function, &queue);
    }

    sleep(5);

    printf("Stopping workers...\n");

    queue_shutdown(&queue);

    for (int i = 0; i < WORKER_COUNT; i++)
    {
        pthread_join(workers[i], NULL);
    }

    queue_destroy(&queue);

    printf("Test completed successfully.\n");

    return 0;
}