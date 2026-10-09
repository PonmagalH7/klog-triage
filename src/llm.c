
#include "llm.h"

#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <unistd.h>

#define OLLAMA_URL "http://localhost:11434/api/generate"
#define OLLAMA_MODEL "qwen2.5:3b"
#define REQUEST_TIMEOUT 60L


static void fallback_explanation(const char *error,
                                char *output,
                                size_t output_size)
{
    const char *message;

    if (strstr(error, "Out of memory") != NULL ||
        strstr(error, "out of memory") != NULL) {
        message =
            "What happened: The system ran low on available memory.\n"
            "Possible cause: High memory usage or insufficient swap space.\n"
            "Suggested solution: Check memory usage and identify processes "
            "consuming excessive memory.";
    } else if (strstr(error, "I/O error") != NULL ||
               strstr(error, "I/O") != NULL) {
        message =
            "What happened: A device input/output operation failed.\n"
            "Possible cause: A storage device, connection, or driver problem.\n"
            "Suggested solution: Check device connections and system logs.";
    } else if (strstr(error, "segfault") != NULL ||
               strstr(error, "Segmentation fault") != NULL) {
        message =
            "What happened: A process attempted an invalid memory access.\n"
            "Possible cause: An invalid pointer or memory access bug.\n"
            "Suggested solution: Check the process logs and debug the program.";
    } else {
        message =
            "What happened: A kernel error was reported.\n"
            "Possible cause: The specific cause could not be determined "
            "without further analysis.\n"
            "Suggested solution: Check the kernel logs and investigate "
            "the related device or process.";
    }

    snprintf(output, output_size, "%s", message);
}


static int json_escape(const char *input, char *output, size_t output_size)
{
    size_t j = 0;

    for (size_t i = 0; input[i] != '\0'; i++)
    {
        unsigned char c = (unsigned char)input[i];

        if (c == '"' || c == '\\')
        {
            if (j + 2 >= output_size)
            {
                return 0;
            }
            output[j++] = '\\';
            output[j++] = (char)c;
        }
        else if (c == '\n' || c == '\r' || c == '\t')
        {
            if (j + 2 >= output_size)
            {
                return 0;
            }
            output[j++] = '\\';
            output[j++] = (c == '\n') ? 'n' : (c == '\r') ? 'r'
                                                          : 't';
        }
        else if (c < 0x20)
        {
            return 0;
        }
        else
        {
            if (j + 1 >= output_size)
            {
                return 0;
            }
            output[j++] = (char)c;
        }
    }

    if (output_size == 0)
    {
        return 0;
    }

    output[j] = '\0';
    return 1;
}

struct ResponseBuffer
{
    char *data;
    size_t size;
    size_t capacity;
};

static size_t write_callback(void *contents, size_t size,
                             size_t nmemb, void *user_data)
{
    size_t bytes = size * nmemb;
    struct ResponseBuffer *buffer = user_data;

    if (buffer->size + bytes + 1 > buffer->capacity)
    {
        return 0;
    }

    memcpy(buffer->data + buffer->size, contents, bytes);
    buffer->size += bytes;
    buffer->data[buffer->size] = '\0';

    return bytes;
}

int llm_explain(const char *kernel_error,
                char *output,
                size_t output_size)
{
    CURL *curl;
    CURLcode result;
    struct curl_slist *headers = NULL;
    char request[4096];
    char response[8192];
    char escaped_error[2048];
    struct ResponseBuffer buffer = {
        response, 0, sizeof(response)};

    if (kernel_error == NULL || output == NULL || output_size == 0)
    {
        return 0;
    }

    output[0] = '\0';
    if (!json_escape(kernel_error, escaped_error, sizeof(escaped_error)))
    {
        return 0;
    }
    response[0] = '\0';

    int written = snprintf(
        request, sizeof(request),
        "{\"model\":\"%s\",\"stream\":false,"
        "\"prompt\":\"Explain this Linux kernel error in simple terms. "
        "Return exactly these three sections: What happened, Possible cause, "
        "Suggested solution. Error: %s\"}",
        OLLAMA_MODEL, escaped_error);

    if (written < 0 || (size_t)written >= sizeof(request))
    {
        return 0;
    }

    curl = curl_easy_init();
    if (curl == NULL)
    {
        return 0;
    }

    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, OLLAMA_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, REQUEST_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

    long http_status = 0;
    int attempts = 0;

    do
    {
        result = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);

        if (result == CURLE_OK && http_status == 200)
        {
            break;
        }

        attempts++;

        if (attempts < 2)
        {
            sleep(1);
        }
    } while (attempts < 2);

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK || http_status != 200) {
        fallback_explanation(kernel_error, output, output_size);
        return 1;
    }

    const char *key = "\"response\":\"";
    char *start = strstr(response, key);

    if (start == NULL)
    {
        return 0;
    }

    start += strlen(key);

    size_t out = 0;
    while (*start != '\0' && out + 1 < output_size)
    {
        if (*start == '"')
        {
            break;
        }

        if (*start == '\\' && start[1] != '\0')
        {
            start++;

            if (*start == 'n')
            {
                output[out++] = '\n';
            }
            else if (*start == 't')
            {
                output[out++] = '\t';
            }
            else
            {
                output[out++] = *start;
            }

            start++;
        }
        else
        {
            output[out++] = *start++;
        }
    }

    output[out] = '\0';
    return out > 0;
}
