#ifndef T3_MD5_H
#define T3_MD5_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MD5_DIGEST_SIZE 16
#define MD5_HEX_SIZE    33  /* 32 chars + '\0' */

typedef struct {
    uint32_t state[4];
    uint32_t count[2];
    uint8_t  buffer[64];
} md5_ctx_t;

void md5_init(md5_ctx_t *ctx);
void md5_update(md5_ctx_t *ctx, const uint8_t *data, size_t len);
void md5_final(md5_ctx_t *ctx, uint8_t digest[MD5_DIGEST_SIZE]);

/* 便捷函数：计算 data 的 MD5，返回 32 位小写十六进制字符串 */
void md5_hex(const void *data, size_t len, char out[MD5_HEX_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* T3_MD5_H */
