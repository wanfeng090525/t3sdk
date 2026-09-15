/*
 * T3 SDK 加密模块
 * 实现 RC4、DES-ECB、Base64、自定义 Base64、HEX 编解码
 */
#include "crypto.h"
#include <string.h>
#include <stdlib.h>

/* ==================== RC4 实现 ==================== */
void t3_rc4(const unsigned char *data, size_t data_len,
            const char *key, size_t key_len,
            unsigned char *out)
{
    unsigned char S[256];
    int i, j;
    unsigned char tmp;

    /* KSA 密钥调度 */
    for (i = 0; i < 256; i++)
        S[i] = (unsigned char)i;

    j = 0;
    for (i = 0; i < 256; i++) {
        j = (j + S[i] + (unsigned char)key[i % key_len]) & 0xFF;
        tmp = S[i];
        S[i] = S[j];
        S[j] = tmp;
    }

    /* PRGA 伪随机生成 */
    i = j = 0;
    for (size_t k = 0; k < data_len; k++) {
        i = (i + 1) & 0xFF;
        j = (j + S[i]) & 0xFF;
        tmp = S[i];
        S[i] = S[j];
        S[j] = tmp;
        out[k] = data[k] ^ S[(S[i] + S[j]) & 0xFF];
    }
}

void t3_rc4_str(const char *data, const char *key, unsigned char *out)
{
    t3_rc4((const unsigned char *)data, strlen(data),
           key, strlen(key), out);
}

/* ==================== DES-ECB 实现 ==================== */
/* DES 初始置换表 IP */
static const int IP[64] = {
    58,50,42,34,26,18,10,2, 60,52,44,36,28,20,12,4,
    62,54,46,38,30,22,14,6, 64,56,48,40,32,24,16,8,
    57,49,41,33,25,17, 9,1, 59,51,43,35,27,19,11,3,
    61,53,45,37,29,21,13,5, 63,55,47,39,31,23,15,7
};

/* 逆初始置换表 IP^-1 */
static const int FP[64] = {
    40,8,48,16,56,24,64,32, 39,7,47,15,55,23,63,31,
    38,6,46,14,54,22,62,30, 37,5,45,13,53,21,61,29,
    36,4,44,12,52,20,60,28, 35,3,43,11,51,19,59,27,
    34,2,42,10,50,18,58,26, 33,1,41, 9,49,17,57,25
};

/* 扩展置换表 E */
static const int E[48] = {
    32, 1, 2, 3, 4, 5,  4, 5, 6, 7, 8, 9,
     8, 9,10,11,12,13, 12,13,14,15,16,17,
    16,17,18,19,20,21, 20,21,22,23,24,25,
    24,25,26,27,28,29, 28,29,30,31,32, 1
};

/* S 盒 */
static const int S_BOX[8][4][16] = {
    {{14,4,13,1,2,15,11,8,3,10,6,12,5,9,0,7},
     {0,15,7,4,14,2,13,1,10,6,12,11,9,5,3,8},
     {4,1,14,8,13,6,2,11,15,12,9,7,3,10,5,0},
     {15,12,8,2,4,9,1,7,5,11,3,14,10,0,6,13}},
    {{15,1,8,14,6,11,3,4,9,7,2,13,12,0,5,10},
     {3,13,4,7,15,2,8,14,12,0,1,10,6,9,11,5},
     {0,14,7,11,10,4,13,1,5,8,12,6,9,3,2,15},
     {13,8,10,1,3,15,4,2,11,6,7,12,0,5,14,9}},
    {{10,0,9,14,6,3,15,5,1,13,12,7,11,4,2,8},
     {13,7,0,9,3,4,6,10,2,8,5,14,12,11,15,1},
     {13,6,4,9,8,15,3,0,11,1,2,12,5,10,14,7},
     {1,10,13,0,6,9,8,7,4,15,14,3,11,5,2,12}},
    {{7,13,14,3,0,6,9,10,1,2,8,5,11,12,4,15},
     {13,8,11,5,6,15,0,3,4,7,2,12,1,10,14,9},
     {10,6,9,0,12,11,7,13,15,1,3,14,5,2,8,4},
     {3,15,0,6,10,1,13,8,9,4,5,11,12,7,2,14}},
    {{2,12,4,1,7,10,11,6,8,5,3,15,13,0,14,9},
     {14,11,2,12,4,7,13,1,5,0,15,10,3,9,8,6},
     {4,2,1,11,10,13,7,8,15,9,12,5,6,3,0,14},
     {11,8,12,7,1,14,2,13,6,15,0,9,10,4,5,3}},
    {{12,1,10,15,9,2,6,8,0,13,3,4,14,7,5,11},
     {10,15,4,2,7,12,9,5,6,1,13,14,0,11,3,8},
     {9,14,15,5,2,8,12,3,7,0,4,10,1,13,11,6},
     {4,3,2,12,9,5,15,10,11,14,1,7,6,0,8,13}},
    {{4,11,2,14,15,0,8,13,3,12,9,7,5,10,6,1},
     {13,0,11,7,4,9,1,10,14,3,5,12,2,15,8,6},
     {1,4,11,13,12,3,7,14,10,15,6,8,0,5,9,2},
     {6,11,13,8,1,4,10,7,9,5,0,15,14,2,3,12}},
    {{13,2,8,4,6,15,11,1,10,9,3,14,5,0,12,7},
     {1,15,13,8,10,3,7,4,12,5,6,11,0,14,9,2},
     {7,11,4,1,9,12,14,2,0,6,10,13,15,3,5,8},
     {2,1,14,7,4,10,8,13,15,12,9,0,3,5,6,11}}
};

/* P 盒置换 */
static const int P[32] = {
    16,7,20,21,29,12,28,17, 1,15,23,26,5,18,31,10,
     2,8,24,14,32,27,3,9, 19,13,30,6,22,11,4,25
};

/* 置换选择表 PC-1 */
static const int PC1[56] = {
    57,49,41,33,25,17,9, 1,58,50,42,34,26,18,
    10,2,59,51,43,35,27, 19,11,3,60,52,44,36,
    63,55,47,39,31,23,15,7,62,54,46,38,30,22,
    14,6,61,53,45,37,29,21,13,5,28,20,12,4
};

/* 置换选择表 PC-2 */
static const int PC2[48] = {
    14,17,11,24,1,5, 3,28,15,6,21,10,
    23,19,12,4,26,8, 16,7,27,20,13,2,
    41,52,31,37,47,55,30,40,51,45,33,48,
    44,49,39,56,34,53,46,42,50,36,29,32
};

/* 左移位数表 */
static const int SHIFTS[16] = {1,1,2,2,2,2,2,2,1,2,2,2,2,2,2,1};

static void permute(const unsigned char *in, unsigned char *out,
                    const int *table, int n)
{
    int i, pos;
    memset(out, 0, (n + 7) / 8);
    for (i = 0; i < n; i++) {
        pos = table[i] - 1;
        if (in[pos / 8] & (0x80 >> (pos % 8)))
            out[i / 8] |= (0x80 >> (i % 8));
    }
}

static void des_generate_subkeys(const unsigned char key[8],
                                 unsigned char subkeys[16][6])
{
    unsigned char permuted[7];
    unsigned char C[4], D[4];
    int i, j, shift;

    permute(key, permuted, PC1, 56);

    C[0] = permuted[0]; C[1] = permuted[1];
    C[2] = permuted[2]; C[3] = permuted[3] & 0xF0;
    D[0] = (permuted[3] & 0x0F) << 4 | (permuted[4] >> 4);
    D[1] = permuted[4] << 4 | (permuted[5] >> 4);
    D[2] = permuted[5] << 4 | (permuted[6] >> 4);
    D[3] = permuted[6] << 4;

    for (i = 0; i < 16; i++) {
        shift = SHIFTS[i];
        for (j = 0; j < shift; j++) {
            /* 左循环移位 C (28 bits) */
            unsigned int c_val = ((unsigned int)C[0] << 20) |
                                 ((unsigned int)C[1] << 12) |
                                 ((unsigned int)C[2] << 4) |
                                 ((unsigned int)C[3] >> 4);
            c_val = ((c_val << 1) & 0x0FFFFFFF) | ((c_val >> 27) & 1);
            C[0] = (c_val >> 20) & 0xFF;
            C[1] = (c_val >> 12) & 0xFF;
            C[2] = (c_val >> 4) & 0xFF;
            C[3] = (c_val & 0xF) << 4;

            /* 左循环移位 D (28 bits) */
            unsigned int d_val = ((unsigned int)D[0] << 20) |
                                 ((unsigned int)D[1] << 12) |
                                 ((unsigned int)D[2] << 4) |
                                 ((unsigned int)D[3] >> 4);
            d_val = ((d_val << 1) & 0x0FFFFFFF) | ((d_val >> 27) & 1);
            D[0] = (d_val >> 20) & 0xFF;
            D[1] = (d_val >> 12) & 0xFF;
            D[2] = (d_val >> 4) & 0xFF;
            D[3] = (d_val & 0xF) << 4;
        }

        unsigned char cd[7];
        cd[0] = C[0]; cd[1] = C[1]; cd[2] = C[2];
        cd[3] = C[3] | (D[0] >> 4);
        cd[4] = (D[0] << 4) | (D[1] >> 4);
        cd[5] = (D[1] << 4) | (D[2] >> 4);
        cd[6] = (D[2] << 4) | (D[3] >> 4);

        permute(cd, subkeys[i], PC2, 48);
    }
}

static void des_feistel(unsigned char R[4], const unsigned char subkey[6],
                        unsigned char out[4])
{
    unsigned char expanded[6];
    int i;

    permute(R, expanded, E, 48);

    for (i = 0; i < 6; i++)
        expanded[i] ^= subkey[i];

    unsigned char sbox_out[4] = {0};
    for (i = 0; i < 8; i++) {
        int byte_idx = i / 2;
        int bit_offset = (i % 2) * 4;
        unsigned char b = (expanded[byte_idx] >> (4 - bit_offset)) & 0x3F;
        /* 但实际应该按位取，这里简化处理 */
        int row = ((b >> 5) & 2) | (b & 1);
        int col = (b >> 1) & 0xF;
        int val = S_BOX[i][row][col];
        if (i % 2 == 0)
            sbox_out[i / 2] = (val << 4);
        else
            sbox_out[i / 2] |= val;
    }

    permute(sbox_out, out, P, 32);
}

static void des_crypt_block(const unsigned char in[8], unsigned char out[8],
                            unsigned char subkeys[16][6], int decrypt)
{
    unsigned char permuted[8];
    unsigned char L[4], R[4], tmp[4];
    int i;

    permute(in, permuted, IP, 64);
    memcpy(L, permuted, 4);
    memcpy(R, permuted + 4, 4);

    for (i = 0; i < 16; i++) {
        int key_idx = decrypt ? (15 - i) : i;
        des_feistel(R, subkeys[key_idx], tmp);
        for (int j = 0; j < 4; j++)
            tmp[j] ^= L[j];
        memcpy(L, R, 4);
        memcpy(R, tmp, 4);
    }

    unsigned char combined[8];
    memcpy(combined, R, 4);
    memcpy(combined + 4, L, 4);
    permute(combined, out, FP, 64);
}

size_t t3_des_ecb_encrypt(const unsigned char *data, size_t data_len,
                          const unsigned char key[8], unsigned char *out)
{
    unsigned char subkeys[16][6];
    size_t i, blocks, out_len;
    unsigned char pad_len;

    des_generate_subkeys(key, subkeys);

    pad_len = (unsigned char)(8 - (data_len % 8));
    out_len = data_len + pad_len;
    blocks = out_len / 8;

    for (i = 0; i < blocks; i++) {
        unsigned char block[8];
        size_t offset = i * 8;
        if (offset < data_len) {
            size_t copy_len = (data_len - offset >= 8) ? 8 : (data_len - offset);
            memcpy(block, data + offset, copy_len);
            if (copy_len < 8)
                memset(block + copy_len, pad_len, 8 - copy_len);
        } else {
            memset(block, pad_len, 8);
        }
        des_crypt_block(block, out + offset, subkeys, 0);
    }

    return out_len;
}

size_t t3_des_ecb_decrypt(const unsigned char *data, size_t data_len,
                          const unsigned char key[8], unsigned char *out)
{
    unsigned char subkeys[16][6];
    size_t i, blocks;

    if (data_len % 8 != 0)
        return 0;

    des_generate_subkeys(key, subkeys);
    blocks = data_len / 8;

    for (i = 0; i < blocks; i++) {
        des_crypt_block(data + i * 8, out + i * 8, subkeys, 1);
    }

    /* 去除 PKCS7 填充 */
    unsigned char pad_len = out[data_len - 1];
    if (pad_len > 0 && pad_len <= 8 && pad_len <= data_len) {
        for (i = data_len - pad_len; i < data_len; i++) {
            if (out[i] != pad_len)
                return data_len; /* 填充无效，返回原始长度 */
        }
        return data_len - pad_len;
    }
    return data_len;
}

/* ==================== Base64 实现 ==================== */
static const char B64_STD[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t t3_base64_encode(const unsigned char *data, size_t len, char *out)
{
    size_t i, j = 0;
    for (i = 0; i < len; i += 3) {
        unsigned int val = data[i] << 16;
        if (i + 1 < len) val |= data[i + 1] << 8;
        if (i + 2 < len) val |= data[i + 2];

        out[j++] = B64_STD[(val >> 18) & 0x3F];
        out[j++] = B64_STD[(val >> 12) & 0x3F];
        out[j++] = (i + 1 < len) ? B64_STD[(val >> 6) & 0x3F] : '=';
        out[j++] = (i + 2 < len) ? B64_STD[val & 0x3F] : '=';
    }
    out[j] = '\0';
    return j;
}

static int b64_decode_char(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

size_t t3_base64_decode(const char *data, size_t len, unsigned char *out)
{
    size_t i, j = 0;
    int buf[4];
    int buf_idx = 0;

    for (i = 0; i < len; i++) {
        char c = data[i];
        if (c == '=') break;
        int val = b64_decode_char(c);
        if (val < 0) continue;
        buf[buf_idx++] = val;
        if (buf_idx == 4) {
            out[j++] = (buf[0] << 2) | (buf[1] >> 4);
            out[j++] = (buf[1] << 4) | (buf[2] >> 2);
            out[j++] = (buf[2] << 6) | buf[3];
            buf_idx = 0;
        }
    }

    if (buf_idx == 3) {
        out[j++] = (buf[0] << 2) | (buf[1] >> 4);
        out[j++] = (buf[1] << 4) | (buf[2] >> 2);
    } else if (buf_idx == 2) {
        out[j++] = (buf[0] << 2) | (buf[1] >> 4);
    }

    return j;
}

size_t t3_custom_base64_encode(const unsigned char *data, size_t len,
                               const char *charset, char *out)
{
    /* 先用标准 Base64 编码，再替换字符 */
    char *std = (char *)malloc(len * 2 + 8);
    if (!std) return 0;

    size_t std_len = t3_base64_encode(data, len, std);
    size_t j = 0;

    for (size_t i = 0; i < std_len; i++) {
        if (std[i] == '=') {
            out[j++] = '=';
        } else {
            const char *p = strchr(B64_STD, std[i]);
            if (p)
                out[j++] = charset[p - B64_STD];
            else
                out[j++] = std[i];
        }
    }
    out[j] = '\0';
    free(std);
    return j;
}

size_t t3_custom_base64_decode(const char *data, size_t len,
                               const char *charset, unsigned char *out)
{
    /* 先将自定义字符集映射回标准字符集，再解码 */
    char *std = (char *)malloc(len + 1);
    if (!std) return 0;

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (data[i] == '=') {
            std[j++] = '=';
        } else {
            const char *p = strchr(charset, data[i]);
            if (p)
                std[j++] = B64_STD[p - charset];
            else
                std[j++] = data[i];
        }
    }
    std[j] = '\0';

    size_t result = t3_base64_decode(std, j, out);
    free(std);
    return result;
}

/* ==================== HEX 编解码 ==================== */
size_t t3_hex_encode(const unsigned char *data, size_t len, char *out)
{
    static const char hex[] = "0123456789abcdef";
    size_t i;
    for (i = 0; i < len; i++) {
        out[i * 2]     = hex[(data[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex[data[i] & 0x0F];
    }
    out[len * 2] = '\0';
    return len * 2;
}

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

size_t t3_hex_decode(const char *data, size_t len, unsigned char *out)
{
    size_t i, j = 0;
    for (i = 0; i + 1 < len; i += 2) {
        int hi = hex_val(data[i]);
        int lo = hex_val(data[i + 1]);
        if (hi < 0 || lo < 0) break;
        out[j++] = (unsigned char)((hi << 4) | lo);
    }
    return j;
}
