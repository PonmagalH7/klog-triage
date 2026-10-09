
#include <stdio.h>
#include "src/llm.h"

int main(void)
{
    char explanation[4096];

    int success = llm_explain(
        "Out of memory: Kill process",
        explanation,
        sizeof(explanation)
    );

    if (success) {
        printf("Kernel Error Explanation:\n%s\n", explanation);
    } else {
        printf("Failed to get explanation from Ollama.\n");
    }

    return 0;
}
