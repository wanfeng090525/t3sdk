/*
 * T3 SDK HTTP 客户端模块
 * 基于 libcurl 实现
 */
#include "http.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char   *data;
    size_t  size;
} t3_http_buffer_t;

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    t3_http_buffer_t *mem = (t3_http_buffer_t *)userp;

    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr)
        return 0;

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = '\0';

    return realsize;
}

int t3_http_post(const char *url, const char *post_data,
                 char **response, size_t *response_len)
{
    CURL *curl;
    CURLcode res;
    t3_http_buffer_t buf = {0};

    buf.data = malloc(1);
    if (!buf.data)
        return -1;
    buf.size = 0;

    curl = curl_easy_init();
    if (!curl) {
        free(buf.data);
        return -2;
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(buf.data);
        return (int)res;
    }

    *response = buf.data;
    *response_len = buf.size;
    return 0;
}

char *t3_url_encode(const char *str)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return strdup(str);

    char *encoded = curl_easy_escape(curl, str, 0);
    curl_easy_cleanup(curl);

    if (!encoded)
        return strdup(str);

    char *result = strdup(encoded);
    curl_free(encoded);
    return result;
}
