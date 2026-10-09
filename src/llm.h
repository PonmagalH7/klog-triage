
#ifndef LLM_H
#define LLM_H

#include <stddef.h>

int llm_explain(const char *kernel_error,
                char *output,
                size_t output_size);

#endif
