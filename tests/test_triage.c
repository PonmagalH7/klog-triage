#include <stdio.h>
#include "../src/triage.h"

int main(void)
{
    printf("Level 3: %s\n", should_triage(3) ? "TRIAGE" : "IGNORE");
    printf("Level 4: %s\n", should_triage(4) ? "TRIAGE" : "IGNORE");
    printf("Level 6: %s\n", should_triage(6) ? "TRIAGE" : "IGNORE");

    return 0;
}
