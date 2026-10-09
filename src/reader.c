#include "triage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int main(void) {
    int fd = open("/dev/kmsg", O_RDONLY);

    if (fd < 0) {
        perror("open /dev/kmsg (try sudo)");
        return 1;
    }

    char buf[8192];

    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf) - 1);

        if (n < 0) {
            if (errno == EPIPE || errno == EINTR)
                continue;

            perror("read");
            break;
        }

        buf[n] = '\0';

        char *semi = strchr(buf, ';');

        if (!semi)
            continue;

        *semi = '\0';

        unsigned pri;
        unsigned long long seq, ts_us;

        if (sscanf(buf, "%u,%llu,%llu", &pri, &seq, &ts_us) != 3)
            continue;

        char *msg = semi + 1;

        char *nl = strchr(msg, '\n');

        if (nl)
            *nl = '\0';

        unsigned level = pri & 7;
if (should_triage(level)) {
    printf("[level=%u seq=%llu t=%.6f] %s\n",
           level, seq, ts_us / 1e6, msg);
    fflush(stdout);
}
    }

    close(fd);
    return 0;
}
