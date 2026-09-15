#ifndef T3_HTTP_H
#define T3_HTTP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* HTTP POST 请求
 * url: 请求地址
 * post_data: POST 数据（application/x-www-form-urlencoded 格式）
 * response: 输出缓冲区指针，调用者需 free
 * response_len: 输出数据长度
 * 返回: 0 成功，非 0 失败
 */
int t3_http_post(const char *url, const char *post_data,
                 char **response, size_t *response_len);

/* URL 编码 */
char *t3_url_encode(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* T3_HTTP_H */
