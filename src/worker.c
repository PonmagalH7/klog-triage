#include "queue.h"
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#define WORKER_COUNT 3

void *worker_function(void *arg)
{
    PriorityQueue *queue = (PriorityQueue *)arg;

    while (1)
    {
        KernelEvent event;

        if (queue_pop(queue, &event) == -1)
            break;
            
        printf("[Worker %lu] Processing level=%u seq=%llu: %s\n",
               (unsigned long)pthread_self(),
               event.level,
               event.seq,
               event.message);

        fflush(stdout);

        sleep(1);
    }

    return NULL;
}