#ifndef T3_JSON_H
#define T3_JSON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 极简 JSON 解析器 - 仅支持 T3 API 响应格式
 * 支持对象 { "key": "value" } 形式，值为字符串或数字
 */

/* 从 JSON 字符串中获取字符串值
 * 返回值指向 out 缓冲区，失败返回 NULL
 */
char *t3_json_get_string(const char *json, const char *key, char *out, size_t out_size);

/* 从 JSON 字符串中获取整数值
 * 成功返回 0，失败返回 -1
 */
int t3_json_get_int(const char *json, const char *key, int *out);

#ifdef __cplusplus
}
#endif

#endif /* T3_JSON_H */
