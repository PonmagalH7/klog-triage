
#include <stdio.h>
#include "src/llm.h"

int main(void)
{
    const char *errors[] = {
        "Out of memory: Kill process",
        "Kernel panic - not syncing",
        "I/O error on device sda",
        "segfault at address 0x0",
        "USB device not accepting address",
        "EXT4-fs error",
        "NETDEV WATCHDOG: transmit queue timed out",
        "CPU temperature above threshold",
        "Read-only file system",
        "Kernel warning: device failed"
    };

    int total = sizeof(errors) / sizeof(errors[0]);

    for (int i = 0; i < total; i++) {
        char explanation[4096];

        printf("\n====================================\n");
        printf("Test %d: %s\n", i + 1, errors[i]);
        printf("====================================\n");

        if (llm_explain(errors[i], explanation, sizeof(explanation))) {
            printf("%s\n", explanation);
        } else {
            printf("No explanation could be generated.\n");
        }
    }

    return 0;
}
