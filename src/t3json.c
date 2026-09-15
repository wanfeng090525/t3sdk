/*
 * T3 SDK 极简 JSON 解析器
 * 仅用于解析 T3 API 响应格式
 */
#include "t3json.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

static const char *find_key(const char *json, const char *key)
{
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);

    const char *p = strstr(json, search_key);
    if (!p)
        return NULL;

    p += strlen(search_key);

    /* 跳过空白和冒号 */
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ':'))
        p++;

    return p;
}

char *t3_json_get_string(const char *json, const char *key, char *out, size_t out_size)
{
    if (!json || !key || !out || out_size == 0)
        return NULL;

    const char *p = find_key(json, key);
    if (!p)
        return NULL;

    /* 值必须以引号开头 */
    if (*p != '"')
        return NULL;
    p++;

    size_t i = 0;
    while (*p && *p != '"' && i < out_size - 1) {
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
                case 'n': out[i++] = '\n'; break;
                case 't': out[i++] = '\t'; break;
                case 'r': out[i++] = '\r'; break;
                case '"': out[i++] = '"'; break;
                case '\\': out[i++] = '\\'; break;
                case '/': out[i++] = '/'; break;
                default: out[i++] = *p; break;
            }
        } else {
            out[i++] = *p;
        }
        p++;
    }
    out[i] = '\0';

    return out;
}

int t3_json_get_int(const char *json, const char *key, int *out)
{
    if (!json || !key || !out)
        return -1;

    const char *p = find_key(json, key);
    if (!p)
        return -1;

    /* 跳过前导空白 */
    while (*p && isspace((unsigned char)*p))
        p++;

    /* 可能有引号（字符串类型的数字） */
    if (*p == '"')
        p++;

    if (!isdigit((unsigned char)*p) && *p != '-')
        return -1;

    *out = atoi(p);
    return 0;
}
