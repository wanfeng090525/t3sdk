#ifndef T3_CRYPTO_H
#define T3_CRYPTO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== RC4 对称流式加密 ==================== */
/* RC4 加密和解密使用同一函数同一密钥 */
void t3_rc4(const unsigned char *data, size_t data_len,
            const char *key, size_t key_len,
            unsigned char *out);

/* 便捷函数：对字符串执行 RC4，输出原始字节
 * 调用者需保证 out 缓冲区 >= data_len
 */
void t3_rc4_str(const char *data, const char *key, unsigned char *out);

/* ==================== DES-ECB 对称分组加密 ==================== */
/* DES-ECB + PKCS7 填充，密钥固定 8 字节 */
/* 返回加密后的数据长度（含填充），out 缓冲区需 >= (data_len/8 + 1)*8 */
size_t t3_des_ecb_encrypt(const unsigned char *data, size_t data_len,
                          const unsigned char key[8], unsigned char *out);

/* 解密，返回解密后的原始数据长度 */
size_t t3_des_ecb_decrypt(const unsigned char *data, size_t data_len,
                          const unsigned char key[8], unsigned char *out);

/* ==================== Base64 编解码 ==================== */
/* 标准 Base64 编码，返回编码后字符串长度（不含 '\0'） */
size_t t3_base64_encode(const unsigned char *data, size_t len, char *out);

/* 标准 Base64 解码，返回解码后数据长度 */
size_t t3_base64_decode(const char *data, size_t len, unsigned char *out);

/* 自定义 Base64 编码（使用 64 字符自定义编码集） */
size_t t3_custom_base64_encode(const unsigned char *data, size_t len,
                               const char *charset, char *out);

/* 自定义 Base64 解码 */
size_t t3_custom_base64_decode(const char *data, size_t len,
                               const char *charset, unsigned char *out);

/* ==================== HEX 编解码 ==================== */
/* 将二进制数据编码为十六进制字符串（小写） */
size_t t3_hex_encode(const unsigned char *data, size_t len, char *out);

/* 将十六进制字符串解码为二进制数据 */
size_t t3_hex_decode(const char *data, size_t len, unsigned char *out);

#ifdef __cplusplus
}
#endif

#endif /* T3_CRYPTO_H */
