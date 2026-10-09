
#include "../src/queue.h"
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    PriorityQueue queue;
    KernelEvent result;

    queue_init(&queue);

    KernelEvent low = {
        .level = 4,
        .seq = 201,
        .ts_us = 1000,
        .message = "Low-priority event"
    };

    KernelEvent high = {
        .level = 1,
        .seq = 202,
        .ts_us = 2000,
        .message = "High-priority event"
    };

    queue_push(&queue, low);

    printf("Added low-priority event: level 4\n");
    printf("Waiting 21 seconds for aging...\n");

    sleep(21);

    queue_push(&queue, high);

    printf("Added high-priority event: level 1\n");

    queue_pop(&queue, &result);

    printf("First event selected: level=%u, seq=%llu, message=%s\n",
           result.level, result.seq, result.message);

    queue_shutdown(&queue);
    queue_destroy(&queue);

    return 0;
}
