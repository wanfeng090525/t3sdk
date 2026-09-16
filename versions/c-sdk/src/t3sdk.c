/**
 * T3验证SDK - C语言版本实现
 */

#include "t3sdk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#define ALL_SERVERS_UNAVAILABLE "无法连接到所有T3网络验证服务器，可能是因为您的网络问题或T3网络验证服务器被攻击造成的，建议检查网络或稍后重试"

static const char *T3_SERVER_URLS[T3_SERVER_COUNT] = {
    "https://w.t3yanzheng.com/", "https://w2.t3yanzheng.com/", "https://w3.t3yanzheng.com/",
    "https://w4.t3yanzheng.com/", "https://w5.t3yanzheng.com/", "https://w.t3data.net/"
};

/* 平台检测 */
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "iphlpapi.lib")
    #include <iphlpapi.h>
#else
    #include <sys/socket.h>
    #include <sys/time.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <sys/ioctl.h>
    #include <net/if.h>
    #ifdef __APPLE__
        #include <sys/sysctl.h>
        #include <net/if_dl.h>
    #else
        #include <linux/if_packet.h>
    #endif
#endif

/* ========== DNS 缓存 ==========
 * 每次请求调用 gethostbyname 会阻塞且无超时，网络差时单次请求可能卡数秒。
 * 缓存解析结果（TTL 10 分钟），心跳/登录/解绑等高频请求直接复用 IP，大幅提速。
 */
#define DNS_CACHE_CAP 8
#define DNS_CACHE_TTL 600

static struct {
    char host[256];
    struct in_addr addr;
    time_t ts;
} g_dns_cache[DNS_CACHE_CAP];
static int g_dns_cache_count = 0;

static int resolve_host_cached(const char *host, struct in_addr *out) {
    time_t now = time(NULL);
    int i;
    for (i = 0; i < g_dns_cache_count; i++) {
        if (strcmp(g_dns_cache[i].host, host) == 0) {
            if (now - g_dns_cache[i].ts < DNS_CACHE_TTL) {
                *out = g_dns_cache[i].addr;
                return 0;
            }
            /* 缓存过期，移除后重新解析 */
            g_dns_cache[i] = g_dns_cache[--g_dns_cache_count];
            break;
        }
    }
    struct hostent *h = gethostbyname(host);
    if (h == NULL) return -1;
    memcpy(out, h->h_addr, h->h_length);
    if (g_dns_cache_count < DNS_CACHE_CAP) {
        strncpy(g_dns_cache[g_dns_cache_count].host, host, 255);
        g_dns_cache[g_dns_cache_count].host[255] = '\0';
        g_dns_cache[g_dns_cache_count].addr = *out;
        g_dns_cache[g_dns_cache_count].ts = now;
        g_dns_cache_count++;
    }
    return 0;
}

/* ========== MD5算法实现 ========== */

/* MD5上下文 */
typedef struct {
    uint32_t state[4];      /* 状态(ABCD) */
    uint32_t count[2];      /* 比特数，模2^64 (低位优先) */
    unsigned char buffer[64]; /* 输入缓冲区 */
} MD5_CTX;

/* MD5基本变换常量 */
#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

/* MD5基本函数 */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* 循环左移 */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* MD5变换宏 */
#define FF(a, b, c, d, x, s, ac) { \
    (a) += F((b), (c), (d)) + (x) + (uint32_t)(ac); \
    (a) = ROTATE_LEFT((a), (s)); \
    (a) += (b); \
}
#define GG(a, b, c, d, x, s, ac) { \
    (a) += G((b), (c), (d)) + (x) + (uint32_t)(ac); \
    (a) = ROTATE_LEFT((a), (s)); \
    (a) += (b); \
}
#define HH(a, b, c, d, x, s, ac) { \
    (a) += H((b), (c), (d)) + (x) + (uint32_t)(ac); \
    (a) = ROTATE_LEFT((a), (s)); \
    (a) += (b); \
}
#define II(a, b, c, d, x, s, ac) { \
    (a) += I((b), (c), (d)) + (x) + (uint32_t)(ac); \
    (a) = ROTATE_LEFT((a), (s)); \
    (a) += (b); \
}

/* 前向声明 */
static void MD5Init(MD5_CTX *context);
static void MD5Update(MD5_CTX *context, const unsigned char *input, unsigned int inputLen);
static void MD5Final(unsigned char digest[16], MD5_CTX *context);
static void MD5Transform(uint32_t state[4], const unsigned char block[64]);
static void Encode(unsigned char *output, const uint32_t *input, unsigned int len);
static void Decode(uint32_t *output, const unsigned char *input, unsigned int len);

static unsigned char PADDING[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* MD5初始化 */
static void MD5Init(MD5_CTX *context) {
    context->count[0] = context->count[1] = 0;
    context->state[0] = 0x67452301;
    context->state[1] = 0xefcdab89;
    context->state[2] = 0x98badcfe;
    context->state[3] = 0x10325476;
}

/* MD5块处理 */
static void MD5Update(MD5_CTX *context, const unsigned char *input, unsigned int inputLen) {
    unsigned int i, index, partLen;
    
    index = (unsigned int)((context->count[0] >> 3) & 0x3F);
    
    if ((context->count[0] += ((uint32_t)inputLen << 3)) < ((uint32_t)inputLen << 3))
        context->count[1]++;
    context->count[1] += ((uint32_t)inputLen >> 29);
    
    partLen = 64 - index;
    
    if (inputLen >= partLen) {
        memcpy(&context->buffer[index], input, partLen);
        MD5Transform(context->state, context->buffer);
        
        for (i = partLen; i + 63 < inputLen; i += 64)
            MD5Transform(context->state, &input[i]);
        
        index = 0;
    } else
        i = 0;
    
    memcpy(&context->buffer[index], &input[i], inputLen - i);
}

/* MD5最终处理 */
static void MD5Final(unsigned char digest[16], MD5_CTX *context) {
    unsigned char bits[8];
    unsigned int index, padLen;
    
    Encode(bits, context->count, 8);
    
    index = (unsigned int)((context->count[0] >> 3) & 0x3f);
    padLen = (index < 56) ? (56 - index) : (120 - index);
    MD5Update(context, PADDING, padLen);
    
    MD5Update(context, bits, 8);
    
    Encode(digest, context->state, 16);
    
    memset(context, 0, sizeof(*context));
}

/* MD5核心变换 */
static void MD5Transform(uint32_t state[4], const unsigned char block[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];
    
    Decode(x, block, 64);
    
    /* 第1轮 */
    FF(a, b, c, d, x[0], S11, 0xd76aa478);
    FF(d, a, b, c, x[1], S12, 0xe8c7b756);
    FF(c, d, a, b, x[2], S13, 0x242070db);
    FF(b, c, d, a, x[3], S14, 0xc1bdceee);
    FF(a, b, c, d, x[4], S11, 0xf57c0faf);
    FF(d, a, b, c, x[5], S12, 0x4787c62a);
    FF(c, d, a, b, x[6], S13, 0xa8304613);
    FF(b, c, d, a, x[7], S14, 0xfd469501);
    FF(a, b, c, d, x[8], S11, 0x698098d8);
    FF(d, a, b, c, x[9], S12, 0x8b44f7af);
    FF(c, d, a, b, x[10], S13, 0xffff5bb1);
    FF(b, c, d, a, x[11], S14, 0x895cd7be);
    FF(a, b, c, d, x[12], S11, 0x6b901122);
    FF(d, a, b, c, x[13], S12, 0xfd987193);
    FF(c, d, a, b, x[14], S13, 0xa679438e);
    FF(b, c, d, a, x[15], S14, 0x49b40821);
    
    /* 第2轮 */
    GG(a, b, c, d, x[1], S21, 0xf61e2562);
    GG(d, a, b, c, x[6], S22, 0xc040b340);
    GG(c, d, a, b, x[11], S23, 0x265e5a51);
    GG(b, c, d, a, x[0], S24, 0xe9b6c7aa);
    GG(a, b, c, d, x[5], S21, 0xd62f105d);
    GG(d, a, b, c, x[10], S22, 0x2441453);
    GG(c, d, a, b, x[15], S23, 0xd8a1e681);
    GG(b, c, d, a, x[4], S24, 0xe7d3fbc8);
    GG(a, b, c, d, x[9], S21, 0x21e1cde6);
    GG(d, a, b, c, x[14], S22, 0xc33707d6);
    GG(c, d, a, b, x[3], S23, 0xf4d50d87);
    GG(b, c, d, a, x[8], S24, 0x455a14ed);
    GG(a, b, c, d, x[13], S21, 0xa9e3e905);
    GG(d, a, b, c, x[2], S22, 0xfcefa3f8);
    GG(c, d, a, b, x[7], S23, 0x676f02d9);
    GG(b, c, d, a, x[12], S24, 0x8d2a4c8a);
    
    /* 第3轮 */
    HH(a, b, c, d, x[5], S31, 0xfffa3942);
    HH(d, a, b, c, x[8], S32, 0x8771f681);
    HH(c, d, a, b, x[11], S33, 0x6d9d6122);
    HH(b, c, d, a, x[14], S34, 0xfde5380c);
    HH(a, b, c, d, x[1], S31, 0xa4beea44);
    HH(d, a, b, c, x[4], S32, 0x4bdecfa9);
    HH(c, d, a, b, x[7], S33, 0xf6bb4b60);
    HH(b, c, d, a, x[10], S34, 0xbebfbc70);
    HH(a, b, c, d, x[13], S31, 0x289b7ec6);
    HH(d, a, b, c, x[0], S32, 0xeaa127fa);
    HH(c, d, a, b, x[3], S33, 0xd4ef3085);
    HH(b, c, d, a, x[6], S34, 0x4881d05);
    HH(a, b, c, d, x[9], S31, 0xd9d4d039);
    HH(d, a, b, c, x[12], S32, 0xe6db99e5);
    HH(c, d, a, b, x[15], S33, 0x1fa27cf8);
    HH(b, c, d, a, x[2], S34, 0xc4ac5665);
    
    /* 第4轮 */
    II(a, b, c, d, x[0], S41, 0xf4292244);
    II(d, a, b, c, x[7], S42, 0x432aff97);
    II(c, d, a, b, x[14], S43, 0xab9423a7);
    II(b, c, d, a, x[5], S44, 0xfc93a039);
    II(a, b, c, d, x[12], S41, 0x655b59c3);
    II(d, a, b, c, x[3], S42, 0x8f0ccc92);
    II(c, d, a, b, x[10], S43, 0xffeff47d);
    II(b, c, d, a, x[1], S44, 0x85845dd1);
    II(a, b, c, d, x[8], S41, 0x6fa87e4f);
    II(d, a, b, c, x[15], S42, 0xfe2ce6e0);
    II(c, d, a, b, x[6], S43, 0xa3014314);
    II(b, c, d, a, x[13], S44, 0x4e0811a1);
    II(a, b, c, d, x[4], S41, 0xf7537e82);
    II(d, a, b, c, x[11], S42, 0xbd3af235);
    II(c, d, a, b, x[2], S43, 0x2ad7d2bb);
    II(b, c, d, a, x[9], S44, 0xeb86d391);
    
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    
    memset(x, 0, sizeof(x));
}

/* 将uint32_t编码为字节 */
static void Encode(unsigned char *output, const uint32_t *input, unsigned int len) {
    unsigned int i, j;
    
    for (i = 0, j = 0; j < len; i++, j += 4) {
        output[j] = (unsigned char)(input[i] & 0xff);
        output[j + 1] = (unsigned char)((input[i] >> 8) & 0xff);
        output[j + 2] = (unsigned char)((input[i] >> 16) & 0xff);
        output[j + 3] = (unsigned char)((input[i] >> 24) & 0xff);
    }
}

/* 将字节解码为uint32_t */
static void Decode(uint32_t *output, const unsigned char *input, unsigned int len) {
    unsigned int i, j;
    
    for (i = 0, j = 0; j < len; i++, j += 4)
        output[i] = ((uint32_t)input[j]) | (((uint32_t)input[j + 1]) << 8) |
                   (((uint32_t)input[j + 2]) << 16) | (((uint32_t)input[j + 3]) << 24);
}

/* 计算字符串的MD5哈希(16进制小写) */
static void md5_string(const char *str, char output[33]) {
    MD5_CTX context;
    unsigned char digest[16];
    int i;
    
    MD5Init(&context);
    MD5Update(&context, (const unsigned char *)str, strlen(str));
    MD5Final(digest, &context);
    
    for (i = 0; i < 16; i++) {
        sprintf(output + i * 2, "%02x", digest[i]);
    }
    output[32] = '\0';
}

/* 计算字符串的MD5哈希(16进制大写) */
void md5_string_upper(const char *str, char output[33]) {
    MD5_CTX context;
    unsigned char digest[16];
    int i;
    
    MD5Init(&context);
    MD5Update(&context, (const unsigned char *)str, strlen(str));
    MD5Final(digest, &context);
    
    for (i = 0; i < 16; i++) {
        sprintf(output + i * 2, "%02X", digest[i]);
    }
    output[32] = '\0';
}

/* ========== Base64自定义编码实现 ========== */

/* 标准Base64字符集 */
static const char STANDARD_BASE64_CHARSET[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * Base64编码(使用自定义字符集)
 * 
 * @param data 输入数据
 * @param data_len 数据长度
 * @param custom_charset 自定义字符集(64位)
 * @param output 输出缓冲区
 * @param output_len 输出缓冲区大小
 * @return 编码后的长度，-1表示失败
 */
static int base64_encode_custom(const unsigned char *data, int data_len, 
                                const char *custom_charset, char *output, int output_len) {
    int i, j, val;
    int out_len = ((data_len + 2) / 3) * 4;
    
    if (output_len < out_len + 1) {
        return -1;
    }
    
    /* 标准Base64编码 */
    for (i = 0, j = 0; i < data_len; i += 3) {
        val = data[i] << 16;
        if (i + 1 < data_len) val |= data[i + 1] << 8;
        if (i + 2 < data_len) val |= data[i + 2];
        
        output[j++] = STANDARD_BASE64_CHARSET[(val >> 18) & 0x3F];
        output[j++] = STANDARD_BASE64_CHARSET[(val >> 12) & 0x3F];
        output[j++] = (i + 1 < data_len) ? STANDARD_BASE64_CHARSET[(val >> 6) & 0x3F] : '=';
        output[j++] = (i + 2 < data_len) ? STANDARD_BASE64_CHARSET[val & 0x3F] : '=';
    }
    output[j] = '\0';
    
    /* 转换为自定义字符集 */
    for (i = 0; i < j; i++) {
        if (output[i] != '=') {
            for (int k = 0; k < 64; k++) {
                if (output[i] == STANDARD_BASE64_CHARSET[k]) {
                    output[i] = custom_charset[k];
                    break;
                }
            }
        }
    }
    
    return j;
}

/**
 * Base64解码(使用自定义字符集)
 * 
 * @param data 输入数据
 * @param custom_charset 自定义字符集(64位)
 * @param output 输出缓冲区
 * @param output_len 输出缓冲区大小
 * @return 解码后的长度，-1表示失败
 */
static int base64_decode_custom(const char *data, const char *custom_charset,
                               unsigned char *output, int output_len) {
    int i, j, len;
    char *standard_data;
    unsigned char decode_table[256];
    int val;
    
    len = strlen(data);
    standard_data = (char *)malloc(len + 1);
    if (!standard_data) return -1;
    
    /* 转换为标准字符集 */
    for (i = 0; i < len; i++) {
        if (data[i] == '=') {
            standard_data[i] = '=';
        } else {
            for (j = 0; j < 64; j++) {
                if (data[i] == custom_charset[j]) {
                    standard_data[i] = STANDARD_BASE64_CHARSET[j];
                    break;
                }
            }
        }
    }
    standard_data[len] = '\0';
    
    /* 构建解码表 */
    memset(decode_table, 0xFF, sizeof(decode_table));
    for (i = 0; i < 64; i++) {
        decode_table[(unsigned char)STANDARD_BASE64_CHARSET[i]] = i;
    }
    
    /* 解码 */
    for (i = 0, j = 0; i < len; i += 4) {
        val = (decode_table[(unsigned char)standard_data[i]] << 18) |
              (decode_table[(unsigned char)standard_data[i + 1]] << 12) |
              (decode_table[(unsigned char)standard_data[i + 2]] << 6) |
              decode_table[(unsigned char)standard_data[i + 3]];
        
        if (j >= output_len) {
            free(standard_data);
            return -1;
        }
        
        output[j++] = (val >> 16) & 0xFF;
        if (standard_data[i + 2] != '=' && j < output_len)
            output[j++] = (val >> 8) & 0xFF;
        if (standard_data[i + 3] != '=' && j < output_len)
            output[j++] = val & 0xFF;
    }
    
    free(standard_data);
    return j;
}

/**
 * Base64编码并转换为16进制(使用自定义字符集)
 * 
 * @param data 输入字符串
 * @param custom_charset 自定义字符集(64位)
 * @param output 输出缓冲区
 * @param output_len 输出缓冲区大小
 * @return 0成功，-1失败
 */
static int base64_encode_to_hex(const char *data, const char *custom_charset,
                                char *output, int output_len) {
    char *b64_encoded;
    int b64_len, i;
    int hex_len;
    
    b64_len = ((strlen(data) + 2) / 3) * 4;
    b64_encoded = (char *)malloc(b64_len + 1);
    if (!b64_encoded) return -1;
    
    if (base64_encode_custom((unsigned char *)data, strlen(data), 
                            custom_charset, b64_encoded, b64_len + 1) < 0) {
        free(b64_encoded);
        return -1;
    }
    
    hex_len = strlen(b64_encoded) * 2;
    if (output_len < hex_len + 1) {
        free(b64_encoded);
        return -1;
    }
    
    for (i = 0; i < strlen(b64_encoded); i++) {
        sprintf(output + i * 2, "%02X", (unsigned char)b64_encoded[i]);
    }
    output[hex_len] = '\0';
    
    free(b64_encoded);
    return 0;
}

/* ========== 大数运算(纯C, 无OpenSSL RSA实现) ========== */

#define BN_MAX 72  /* 72*32=2304位, 1024位RSA足够 */

typedef struct { uint32_t w[BN_MAX]; int len; } BN;

static void bn_init(BN *a) { memset(a->w, 0, sizeof(a->w)); a->len = 1; }
static void bn_set(BN *a, uint32_t v) { bn_init(a); a->w[0] = v; }
static void bn_trim(BN *a) { while (a->len > 1 && a->w[a->len-1] == 0) a->len--; }
static int bn_zero(const BN *a) { return a->len == 1 && a->w[0] == 0; }

static void bn_from_be(BN *a, const unsigned char *b, int len) {
    int i; bn_init(a);
    a->len = (len + 3) / 4; if (a->len < 1) a->len = 1;
    if (a->len > BN_MAX) a->len = BN_MAX;
    for (i = 0; i < len; i++) {
        int wi = (len-1-i)/4, bi = (len-1-i)%4;
        if (wi < BN_MAX) a->w[wi] |= ((uint32_t)b[i]) << (bi*8);
    }
    bn_trim(a);
}

static void bn_to_be(const BN *a, unsigned char *b, int len) {
    int i, j; memset(b, 0, len);
    for (i = 0; i < a->len && i*4 < len; i++)
        for (j = 0; j < 4 && i*4+j < len; j++)
            b[len-1-(i*4+j)] = (a->w[i] >> (j*8)) & 0xFF;
}

static int bn_cmp(const BN *a, const BN *b) {
    int i;
    if (a->len != b->len) return a->len > b->len ? 1 : -1;
    for (i = a->len-1; i >= 0; i--)
        if (a->w[i] != b->w[i]) return a->w[i] > b->w[i] ? 1 : -1;
    return 0;
}

static void bn_add(BN *c, const BN *a, const BN *b) {
    int i, ml = a->len > b->len ? a->len : b->len; uint64_t carry = 0;
    for (i = 0; i < ml || carry; i++) {
        uint64_t s = carry;
        if (i < a->len) s += a->w[i]; if (i < b->len) s += b->w[i];
        c->w[i] = (uint32_t)(s & 0xFFFFFFFF); carry = s >> 32;
    }
    c->len = i; bn_trim(c);
}

static void bn_sub(BN *c, const BN *a, const BN *b) {
    int i; int64_t borrow = 0;
    for (i = 0; i < a->len; i++) {
        int64_t d = (int64_t)a->w[i] - borrow;
        if (i < b->len) d -= b->w[i];
        if (d < 0) { d += 0x100000000LL; borrow = 1; } else borrow = 0;
        c->w[i] = (uint32_t)d;
    }
    c->len = a->len; bn_trim(c);
}

static void bn_shl(BN *c, const BN *a, int bits) {
    int ws = bits/32, bs = bits%32, i; uint32_t carry;
    bn_init(c); if (bn_zero(a)) return;
    if (bs == 0) {
        for (i = 0; i < a->len && i+ws < BN_MAX; i++) c->w[i+ws] = a->w[i];
    } else {
        carry = 0;
        for (i = 0; i < a->len && i+ws < BN_MAX; i++) {
            uint64_t v = ((uint64_t)a->w[i] << bs) | carry;
            c->w[i+ws] = (uint32_t)(v & 0xFFFFFFFF); carry = (uint32_t)(v >> 32);
        }
        if (carry && i+ws < BN_MAX) c->w[i+ws] = carry;
    }
    c->len = a->len + ws + 1;
    if (c->len > BN_MAX) c->len = BN_MAX;
    bn_trim(c);
}

static void bn_shr(BN *c, const BN *a, int bits) {
    int ws = bits/32, bs = bits%32, i;
    bn_init(c); if (bn_zero(a) || ws >= a->len) return;
    c->len = a->len - ws;
    if (bs == 0) {
        for (i = 0; i < c->len; i++) c->w[i] = a->w[i+ws];
    } else {
        for (i = 0; i < c->len; i++) {
            c->w[i] = a->w[i+ws] >> bs;
            if (i+ws+1 < a->len) c->w[i] |= a->w[i+ws+1] << (32-bs);
        }
    }
    bn_trim(c);
}

static void bn_mul(BN *c, const BN *a, const BN *b) {
    BN r; int i, j; bn_init(&r);
    r.len = a->len + b->len; if (r.len > BN_MAX) r.len = BN_MAX;
    for (i = 0; i < a->len; i++) {
        uint64_t carry = 0;
        for (j = 0; j < b->len && i+j < BN_MAX; j++) {
            uint64_t p = (uint64_t)a->w[i] * b->w[j] + r.w[i+j] + carry;
            r.w[i+j] = (uint32_t)(p & 0xFFFFFFFF); carry = p >> 32;
        }
        if (carry && i+j < BN_MAX) r.w[i+j] += (uint32_t)carry;
    }
    bn_trim(&r); *c = r;
}

static void bn_mod(BN *rem, const BN *a, const BN *m) {
    BN r, sh; int shift, i;
    r = *a;
    if (bn_zero(m) || bn_cmp(a, m) < 0) { *rem = r; return; }
    shift = 0; { BN t = *m; while (bn_cmp(&t, &r) < 0) { bn_shl(&sh, &t, 1); t = sh; shift++; }
    if (bn_cmp(&t, &r) > 0) shift--; }
    for (i = shift; i >= 0; i--) {
        bn_shl(&sh, m, i);
        if (bn_cmp(&r, &sh) >= 0) { BN nr; bn_sub(&nr, &r, &sh); r = nr; }
    }
    *rem = r;
}

static void bn_modpow(BN *res, const BN *base, const BN *exp, const BN *mod) {
    BN b, e, tmp, mr;
    bn_set(res, 1); bn_mod(&b, base, mod); e = *exp;
    while (!bn_zero(&e)) {
        if (e.w[0] & 1) { bn_mul(&mr, res, &b); bn_mod(res, &mr, mod); }
        bn_mul(&mr, &b, &b); bn_mod(&b, &mr, mod);
        bn_shr(&tmp, &e, 1); e = tmp;
    }
}

/* ========== RSA公钥解析(ASN.1/DER) ========== */

typedef struct { BN n; BN e; int key_size; } RSAPubKey;

static int der_len(const unsigned char *d, int *off) {
    unsigned char f = d[*off]; int n, l = 0, i; (*off)++;
    if (f < 0x80) return f;
    n = f & 0x7F;
    for (i = 0; i < n; i++) { l = (l << 8) | d[*off]; (*off)++; }
    return l;
}

static int parse_rsa_pubkey(const char *pem, RSAPubKey *key) {
    const char *s, *e; char b64[2048]; unsigned char der[1024];
    int dlen, off, alen, nlen, elen, i, j;
    s = strstr(pem, "-----BEGIN PUBLIC KEY-----");
    e = strstr(pem, "-----END PUBLIC KEY-----");
    if (!s || !e) return -1;
    s += strlen("-----BEGIN PUBLIC KEY-----");
    for (i = 0, j = 0; s+i < e && j < (int)sizeof(b64)-1; i++) {
        char c = s[i];
        if (c != '\n' && c != '\r' && c != ' ' && c != '\t') b64[j++] = c;
    }
    b64[j] = '\0';
    dlen = base64_decode_custom(b64, STANDARD_BASE64_CHARSET, der, sizeof(der));
    if (dlen < 0) return -1;
    off = 0;
    if (der[off] != 0x30) return -1; off++; der_len(der, &off);
    if (der[off] != 0x30) return -1; off++; alen = der_len(der, &off); off += alen;
    if (der[off] != 0x03) return -1; off++; der_len(der, &off); off++;
    if (der[off] != 0x30) return -1; off++; der_len(der, &off);
    if (der[off] != 0x02) return -1; off++; nlen = der_len(der, &off);
    if (der[off] == 0x00) { off++; nlen--; }
    bn_from_be(&key->n, der+off, nlen); key->key_size = nlen; off += nlen;
    if (der[off] != 0x02) return -1; off++; elen = der_len(der, &off);
    bn_from_be(&key->e, der+off, elen);
    return 0;
}

/* ========== RSA加密解密(PKCS1 v1.5) ========== */

static int rsa_pad(const unsigned char *d, int dl, unsigned char *o, int ol) {
    int pl, i; if (dl > ol - 11) return -1;
    o[0] = 0x00; o[1] = 0x02; pl = ol - dl - 3;
    for (i = 0; i < pl; i++) { unsigned char v; do { v = rand() % 256; } while (!v); o[2+i] = v; }
    o[2+pl] = 0x00; memcpy(o+3+pl, d, dl); return 0;
}

static int rsa_unpad(const unsigned char *p, int pl, unsigned char *d) {
    int i; if (pl < 11 || p[0] != 0x00) return -1;
    if (p[1] != 0x01 && p[1] != 0x02) return -1;
    i = 2; while (i < pl && p[i] != 0x00) i++;
    if (i >= pl) return -1; i++;
    memcpy(d, p+i, pl-i); return pl-i;
}

/**
 * RSA公钥加密 → HEX大写字符串 (malloc, 调用者free)
 */
static char* rsa_encrypt_hex(const char *msg, const char *pem) {
    RSAPubKey key; int mlen, bsz, nb, total, i; unsigned char *cipher; char *hex;
    if (parse_rsa_pubkey(pem, &key) < 0) return NULL;
    srand((unsigned int)time(NULL));
    mlen = strlen(msg); bsz = key.key_size - 11;
    nb = (mlen + bsz - 1) / bsz; if (nb < 1) nb = 1;
    total = key.key_size * nb;
    cipher = (unsigned char*)malloc(total); if (!cipher) return NULL;
    for (i = 0; i < nb; i++) {
        int off = i*bsz, cur = (off+bsz <= mlen) ? bsz : (mlen-off);
        unsigned char padded[256]; BN m, c;
        if (cur < 0) cur = 0;
        if (rsa_pad((unsigned char*)(msg+off), cur, padded, key.key_size) < 0) { free(cipher); return NULL; }
        bn_from_be(&m, padded, key.key_size);
        bn_modpow(&c, &m, &key.e, &key.n);
        bn_to_be(&c, cipher + i*key.key_size, key.key_size);
    }
    hex = (char*)malloc(total*2+1);
    for (i = 0; i < total; i++) sprintf(hex+i*2, "%02X", cipher[i]);
    hex[total*2] = '\0';
    free(cipher); return hex;
}

/**
 * RSA公钥解密(服务端私钥加密的数据, Base64输入) (malloc, 调用者free)
 */
static char* rsa_decrypt_b64(const char *b64, const char *pem) {
    RSAPubKey key; unsigned char dbuf[4096], obuf[4096];
    int dlen, nb, i, tout; char *res;
    if (parse_rsa_pubkey(pem, &key) < 0) return NULL;
    dlen = base64_decode_custom(b64, STANDARD_BASE64_CHARSET, dbuf, sizeof(dbuf));
    if (dlen < 0) return NULL;
    nb = dlen / key.key_size; tout = 0;
    for (i = 0; i < nb; i++) {
        BN c, m; unsigned char blk[256], up[256]; int ul;
        bn_from_be(&c, dbuf + i*key.key_size, key.key_size);
        bn_modpow(&m, &c, &key.e, &key.n);
        bn_to_be(&m, blk, key.key_size);
        ul = rsa_unpad(blk, key.key_size, up);
        if (ul > 0 && tout+ul < (int)sizeof(obuf)) { memcpy(obuf+tout, up, ul); tout += ul; }
    }
    res = (char*)malloc(tout+1); memcpy(res, obuf, tout); res[tout] = '\0';
    return res;
}

/* ========== HTTP客户端实现 ========== */

/**
 * URL解析结构体
 */
typedef struct {
    char protocol[16];
    char host[256];
    int port;
    char path[512];
} URL_INFO;

/**
 * 解析URL
 */
static int parse_url(const char *url, URL_INFO *info) {
    const char *p = url;
    const char *host_start, *path_start;
    int i;
    
    /* 解析协议 */
    if (strncmp(p, "https://", 8) == 0) {
        strcpy(info->protocol, "https");
        p += 8;
        info->port = 443;
    } else if (strncmp(p, "http://", 7) == 0) {
        strcpy(info->protocol, "http");
        p += 7;
        info->port = 80;
    } else {
        return -1;
    }
    
    /* 解析主机和端口 */
    host_start = p;
    path_start = strchr(p, '/');
    
    if (path_start) {
        i = 0;
        while (host_start < path_start && i < 255) {
            if (*host_start == ':') {
                info->host[i] = '\0';
                info->port = atoi(host_start + 1);
                break;
            }
            info->host[i++] = *host_start++;
        }
        info->host[i] = '\0';
        strcpy(info->path, path_start);
    } else {
        strncpy(info->host, host_start, 255);
        info->host[255] = '\0';
        strcpy(info->path, "/");
    }
    
    return 0;
}

static int connect_with_timeout(int sock, const struct sockaddr *address, int address_len, int seconds) {
    #ifdef _WIN32
    u_long non_blocking = 1;
    int result, socket_error = 0, error_len = sizeof(socket_error);
    fd_set write_set;
    struct timeval timeout;
    ioctlsocket(sock, FIONBIO, &non_blocking);
    result = connect(sock, address, address_len);
    if (result == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
        non_blocking = 0; ioctlsocket(sock, FIONBIO, &non_blocking); return -1;
    }
    FD_ZERO(&write_set); FD_SET(sock, &write_set);
    timeout.tv_sec = seconds; timeout.tv_usec = 0;
    result = select(0, NULL, &write_set, NULL, &timeout);
    if (result > 0) getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&socket_error, &error_len);
    non_blocking = 0; ioctlsocket(sock, FIONBIO, &non_blocking);
    return result > 0 && socket_error == 0 ? 0 : -1;
    #else
    int flags = fcntl(sock, F_GETFL, 0);
    int result, socket_error = 0;
    socklen_t error_len = sizeof(socket_error);
    fd_set write_set;
    struct timeval timeout;
    if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) return -1;
    result = connect(sock, address, (socklen_t)address_len);
    if (result < 0 && errno != EINPROGRESS) { fcntl(sock, F_SETFL, flags); return -1; }
    FD_ZERO(&write_set); FD_SET(sock, &write_set);
    timeout.tv_sec = seconds; timeout.tv_usec = 0;
    result = select(sock + 1, NULL, &write_set, NULL, &timeout);
    if (result > 0) getsockopt(sock, SOL_SOCKET, SO_ERROR, &socket_error, &error_len);
    fcntl(sock, F_SETFL, flags);
    return result > 0 && socket_error == 0 ? 0 : -1;
    #endif
}

/**
 * HTTP POST请求(不支持HTTPS，仅支持HTTP)
 * 注意: T3服务器需要支持HTTP访问，或者用户需要自行添加SSL支持
 */
static int http_post_once(const char *url, const char *post_data, char *response, int response_len) {
    URL_INFO url_info;
    struct sockaddr_in server;
    int sock;
    char request[4096];
    int bytes_received, total_received = 0;
    char *body_start;
    
    #ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return -1;
    }
    #endif
    
    /* 解析URL */
    if (parse_url(url, &url_info) < 0) {
        #ifdef _WIN32
        WSACleanup();
        #endif
        return -1;
    }
    
    /* 对于HTTPS，我们将协议改为HTTP (注意: 生产环境不安全) */
    if (strcmp(url_info.protocol, "https") == 0) {
        /* 此处应该使用SSL/TLS库，但为了避免依赖，我们使用HTTP */
        /* 用户需要确保服务器也支持HTTP，或者自行添加SSL支持 */
        url_info.port = 80;
    }
    
    /* 获取主机信息（带 DNS 缓存，避免每次请求阻塞解析） */
    if (resolve_host_cached(url_info.host, &server.sin_addr) < 0) {
        #ifdef _WIN32
        WSACleanup();
        #endif
        return -1;
    }
    
    /* 创建socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        #ifdef _WIN32
        WSACleanup();
        #endif
        return -1;
    }

    /* 防止单条线路长时间卡住收发 */
    #ifdef _WIN32
    DWORD socket_timeout = 5000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&socket_timeout, sizeof(socket_timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&socket_timeout, sizeof(socket_timeout));
    #else
    struct timeval socket_timeout = {5, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &socket_timeout, sizeof(socket_timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &socket_timeout, sizeof(socket_timeout));
    #endif
    
    /* 设置服务器地址 */
    server.sin_family = AF_INET;
    server.sin_port = htons(url_info.port);
    
    /* 连接服务器 */
    if (connect_with_timeout(sock, (struct sockaddr *)&server, sizeof(server), 2) < 0) {
        #ifdef _WIN32
        closesocket(sock);
        WSACleanup();
        #else
        close(sock);
        #endif
        return -1;
    }
    
    /* 构建HTTP请求 */
    snprintf(request, sizeof(request),
             "POST %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Content-Type: application/x-www-form-urlencoded\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             url_info.path, url_info.host, (int)strlen(post_data), post_data);
    
    /* 发送请求 */
    if (send(sock, request, strlen(request), 0) < 0) {
        #ifdef _WIN32
        closesocket(sock);
        WSACleanup();
        #else
        close(sock);
        #endif
        return -1;
    }
    
    /* 接收响应 */
    memset(response, 0, response_len);
    while ((bytes_received = recv(sock, response + total_received, 
                                  response_len - total_received - 1, 0)) > 0) {
        total_received += bytes_received;
        if (total_received >= response_len - 1) break;
    }
    response[total_received] = '\0';
    
    /* 关闭socket */
    #ifdef _WIN32
    closesocket(sock);
    WSACleanup();
    #else
    close(sock);
    #endif
    
    if (total_received <= 0) return -1;

    /* 408/5xx 视为线路故障 */
    {
        int status = 0;
        sscanf(response, "HTTP/%*s %d", &status);
        if (status == 408 || status >= 500) return -1;
    }

    /* 提取响应体 */
    body_start = strstr(response, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        memmove(response, body_start, strlen(body_start) + 1);
    }
    if (response[0] == '\0') return -1;
    
    return 0;
}

/* ========== JSON解析辅助函数 ========== */

/**
 * 将十六进制字符转换为数字
 */
static int hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/**
 * 将Unicode码点转换为UTF-8编码
 * 
 * @param codepoint Unicode码点
 * @param utf8 输出缓冲区，至少4字节
 * @return 编码后的字节数
 */
static int unicode_to_utf8(unsigned int codepoint, unsigned char *utf8) {
    if (codepoint <= 0x7F) {
        /* 1字节: 0xxxxxxx */
        utf8[0] = (unsigned char)codepoint;
        return 1;
    } else if (codepoint <= 0x7FF) {
        /* 2字节: 110xxxxx 10xxxxxx */
        utf8[0] = (unsigned char)(0xC0 | (codepoint >> 6));
        utf8[1] = (unsigned char)(0x80 | (codepoint & 0x3F));
        return 2;
    } else if (codepoint <= 0xFFFF) {
        /* 3字节: 1110xxxx 10xxxxxx 10xxxxxx */
        utf8[0] = (unsigned char)(0xE0 | (codepoint >> 12));
        utf8[1] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3F));
        utf8[2] = (unsigned char)(0x80 | (codepoint & 0x3F));
        return 3;
    } else if (codepoint <= 0x10FFFF) {
        /* 4字节: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
        utf8[0] = (unsigned char)(0xF0 | (codepoint >> 18));
        utf8[1] = (unsigned char)(0x80 | ((codepoint >> 12) & 0x3F));
        utf8[2] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3F));
        utf8[3] = (unsigned char)(0x80 | (codepoint & 0x3F));
        return 4;
    }
    return 0;
}

/**
 * 解码JSON字符串中的转义序列(包括Unicode转义)
 * 
 * @param input 输入字符串(可能包含\uXXXX等转义)
 * @param output 输出缓冲区
 * @param output_len 输出缓冲区大小
 * @return 0成功，-1失败
 */
static int decode_json_string(const char *input, char *output, int output_len) {
    int out_pos = 0;
    const char *p = input;
    
    while (*p && out_pos < output_len - 1) {
        if (*p == '\\' && *(p + 1)) {
            p++; /* 跳过反斜杠 */
            
            switch (*p) {
                case '"':
                case '\\':
                case '/':
                    output[out_pos++] = *p;
                    break;
                case 'b':
                    output[out_pos++] = '\b';
                    break;
                case 'f':
                    output[out_pos++] = '\f';
                    break;
                case 'n':
                    output[out_pos++] = '\n';
                    break;
                case 'r':
                    output[out_pos++] = '\r';
                    break;
                case 't':
                    output[out_pos++] = '\t';
                    break;
                case 'u': {
                    /* Unicode转义: \uXXXX */
                    if (*(p + 1) && *(p + 2) && *(p + 3) && *(p + 4)) {
                        unsigned int codepoint = 0;
                        int i;
                        
                        /* 解析4位十六进制 */
                        for (i = 0; i < 4; i++) {
                            int hex_val = hex_to_int(*(p + 1 + i));
                            if (hex_val < 0) {
                                output[out_pos] = '\0';
                                return -1;
                            }
                            codepoint = (codepoint << 4) | hex_val;
                        }
                        
                        /* 转换为UTF-8 */
                        unsigned char utf8[4];
                        int utf8_len = unicode_to_utf8(codepoint, utf8);
                        
                        if (utf8_len > 0 && out_pos + utf8_len < output_len) {
                            for (i = 0; i < utf8_len; i++) {
                                output[out_pos++] = utf8[i];
                            }
                        }
                        
                        p += 4; /* 跳过XXXX */
                    }
                    break;
                }
                default:
                    output[out_pos++] = *p;
                    break;
            }
            p++;
        } else {
            output[out_pos++] = *p++;
        }
    }
    
    output[out_pos] = '\0';
    return 0;
}

/**
 * 简单的JSON字符串值提取
 * 注意: 这是一个简化版本，支持提取字符串值和非字符串值（数字、布尔、null）
 */
static int json_get_string(const char *json, const char *key, char *value, int value_len) {
    char search_key[128];
    const char *start, *end;
    int len;
    char temp[MAX_RESPONSE_LEN];
    
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    start = strstr(json, search_key);
    if (!start) return -1;
    
    start = strchr(start, ':');
    if (!start) return -1;
    start++;
    
    while (*start == ' ' || *start == '\t') start++;
    
    if (*start == '"') {
        start++;
        end = strchr(start, '"');
        if (!end) return -1;
        len = end - start;
        if (len >= (int)sizeof(temp)) len = sizeof(temp) - 1;
        strncpy(temp, start, len);
        temp[len] = '\0';
        
        /* 解码转义序列(包括Unicode) */
        if (decode_json_string(temp, value, value_len) < 0) {
            return -1;
        }
        
        return 0;
    }
    
    /* 处理非字符串值（数字、布尔、null） */
    if (strncmp(start, "null", 4) == 0) {
        if (value_len > 0) value[0] = '\0';
        return 0;
    }
    end = start;
    while (*end && *end != ',' && *end != '}' && *end != ']' && *end != '\n') end++;
    len = end - start;
    /* 去除尾部空白 */
    while (len > 0 && (start[len-1] == ' ' || start[len-1] == '\t' || start[len-1] == '\r')) len--;
    if (len <= 0) return -1;
    if (len >= value_len) len = value_len - 1;
    strncpy(value, start, len);
    value[len] = '\0';
    return 0;
}

/**
 * 简单的JSON整数值提取
 */
static int json_get_int(const char *json, const char *key, int *value) {
    char search_key[128];
    const char *start;
    
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    start = strstr(json, search_key);
    if (!start) return -1;
    
    start = strchr(start, ':');
    if (!start) return -1;
    start++;
    
    while (*start == ' ' || *start == '\t') start++;
    
    *value = atoi(start);
    return 0;
}

/* ========== 机器码获取 ========== */

/**
 * 获取机器码
 * Windows: 获取第一个网卡MAC地址
 * Linux: 获取第一个网卡MAC地址
 * macOS: 获取第一个网卡MAC地址
 */
int get_machine_code(char *machine_code) {
    char mac_str[64] = {0};
    
    #ifdef _WIN32
    /* Windows实现 */
    IP_ADAPTER_INFO adapter_info[16];
    DWORD buf_len = sizeof(adapter_info);
    
    if (GetAdaptersInfo(adapter_info, &buf_len) == ERROR_SUCCESS) {
        PIP_ADAPTER_INFO adapter = adapter_info;
        if (adapter) {
            snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                    adapter->Address[0], adapter->Address[1], adapter->Address[2],
                    adapter->Address[3], adapter->Address[4], adapter->Address[5]);
        }
    }
    
    #elif defined(__APPLE__)
    /* macOS实现（与官方C++版一致：获取失败时使用默认值，不返回错误） */
    int mib[6];
    size_t len;
    char *buf = NULL;
    unsigned char *ptr;
    struct if_msghdr *ifm;
    struct sockaddr_dl *sdl;
    
    mib[0] = CTL_NET;
    mib[1] = AF_ROUTE;
    mib[2] = 0;
    mib[3] = AF_LINK;
    mib[4] = NET_RT_IFLIST;
    
    if ((mib[5] = if_nametoindex("en0")) == 0) {
        /* en0 不存在，使用默认值 */
    } else if (sysctl(mib, 6, NULL, &len, NULL, 0) == 0) {
        buf = (char *)malloc(len);
        if (buf != NULL) {
            if (sysctl(mib, 6, buf, &len, NULL, 0) == 0) {
                ifm = (struct if_msghdr *)buf;
                sdl = (struct sockaddr_dl *)(ifm + 1);
                ptr = (unsigned char *)LLADDR(sdl);
                snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                        ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5]);
            }
            free(buf);
        }
    }
    
    #elif defined(__ANDROID__)
    /* Android 实现：遍历所有网络接口获取 MAC 地址
     * 手机网卡名通常是 wlan0(WiFi)/rmnet0(蜂窝)，与桌面端 eth0/ens33 不同
     */
    #include <ifaddrs.h>
    {
        struct ifaddrs *ifaddr = NULL;
        struct ifaddrs *ifa;
        char best_mac[32] = {0};
        int best_priority = 999;

        if (getifaddrs(&ifaddr) == 0) {
            for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_PACKET)
                    continue;
                if (ifa->ifa_flags & IFF_LOOPBACK)
                    continue;

                unsigned char *mac = (unsigned char *)ifa->ifa_addr->sa_data;
                /* 跳过无效 MAC（全 0 或全 FF） */
                int all_zero = 1, all_ff = 1;
                int j;
                for (j = 0; j < 6; j++) {
                    if (mac[j] != 0) all_zero = 0;
                    if (mac[j] != 0xFF) all_ff = 0;
                }
                if (all_zero || all_ff) continue;

                /* 接口优先级: wlan(0) > eth(1) > rmnet(2) > 其他(3) */
                int prio = 3;
                if (strstr(ifa->ifa_name, "wlan")) prio = 0;
                else if (strstr(ifa->ifa_name, "eth")) prio = 1;
                else if (strstr(ifa->ifa_name, "rmnet")) prio = 2;

                if (prio < best_priority) {
                    best_priority = prio;
                    snprintf(best_mac, sizeof(best_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                }
            }
            freeifaddrs(ifaddr);
        }

        if (strlen(best_mac) > 0) {
            snprintf(mac_str, sizeof(mac_str), "%s", best_mac);
        }
    }

    #else
    /* Linux实现（与官方C++版一致：获取失败时使用默认值，不返回错误） */
    struct ifreq ifr;
    int sock;
    int mac_ok = 0;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0) {
        strncpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);
        if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
            /* 尝试其他接口 */
            strncpy(ifr.ifr_name, "ens33", IFNAMSIZ - 1);
            if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
                strncpy(ifr.ifr_name, "enp0s3", IFNAMSIZ - 1);
                if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
                    /* 全部失败，使用默认值 */
                } else {
                    mac_ok = 1;
                }
            } else {
                mac_ok = 1;
            }
        } else {
            mac_ok = 1;
        }
        close(sock);
    }

    if (mac_ok) {
        unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    #endif
    
    /* 如果获取失败，使用默认值 */
    if (strlen(mac_str) == 0) {
        strcpy(mac_str, "00:00:00:00:00:00");
    }
    
    /* 计算MAC地址的MD5哈希 */
    md5_string_upper(mac_str, machine_code);
    
    return 0;
}

/* ========== T3验证SDK核心实现 ========== */

static void init_server_pool(T3Verify *verify) {
    int i;
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)verify ^ (unsigned int)clock();
    verify->server_count = T3_SERVER_COUNT;
    for (i = 0; i < T3_SERVER_COUNT; i++) {
        strncpy(verify->server_urls[i], T3_SERVER_URLS[i], MAX_URL_LEN - 1);
    }
    for (i = T3_SERVER_COUNT - 1; i > 0; i--) {
        char temp[MAX_URL_LEN];
        int j;
        seed = seed * 1664525u + 1013904223u;
        j = (int)(seed % (unsigned int)(i + 1));
        memcpy(temp, verify->server_urls[i], MAX_URL_LEN);
        memcpy(verify->server_urls[i], verify->server_urls[j], MAX_URL_LEN);
        memcpy(verify->server_urls[j], temp, MAX_URL_LEN);
    }
    strncpy(verify->server_url, verify->server_urls[0], MAX_URL_LEN - 1);
}

/**
 * 初始化T3验证SDK
 */
int t3verify_init(
    T3Verify *verify,
    const char *login_code,
    const char *notice_code,
    const char *version_code,
    const char *heartbeat_code,
    const char *appkey,
    const char *base64_charset
) {
    if (!verify || !login_code || !notice_code || !version_code || 
        !heartbeat_code || !appkey || !base64_charset) {
        return -1;
    }
    
    if (strlen(base64_charset) != 64) {
        return -1;
    }
    
    /* 初始化结构体 */
    memset(verify, 0, sizeof(T3Verify));
    
    /* 每个SDK实例随机选择首条线路 */
    init_server_pool(verify);
    
    /* 设置调用码 */
    strncpy(verify->login_code, login_code, MAX_CODE_LEN - 1);
    strncpy(verify->notice_code, notice_code, MAX_CODE_LEN - 1);
    strncpy(verify->version_code, version_code, MAX_CODE_LEN - 1);
    strncpy(verify->heartbeat_code, heartbeat_code, MAX_CODE_LEN - 1);
    
    /* 设置密钥 */
    strncpy(verify->appkey, appkey, MAX_APPKEY_LEN - 1);
    
    /* 设置Base64字符集 */
    strncpy(verify->base64_charset, base64_charset, MAX_CHARSET_LEN - 1);
    
    verify->encode_type = 0; /* Base64模式 */
    verify->initialized = 1;
    
    return 0;
}

/**
 * 初始化T3验证SDK (RSA模式)
 */
int t3verify_init_rsa(
    T3Verify *verify,
    const char *login_code,
    const char *notice_code,
    const char *version_code,
    const char *heartbeat_code,
    const char *appkey,
    const char *rsa_public_key
) {
    if (!verify || !login_code || !notice_code || !version_code || 
        !heartbeat_code || !appkey || !rsa_public_key) {
        return -1;
    }
    memset(verify, 0, sizeof(T3Verify));
    init_server_pool(verify);
    strncpy(verify->login_code, login_code, MAX_CODE_LEN - 1);
    strncpy(verify->notice_code, notice_code, MAX_CODE_LEN - 1);
    strncpy(verify->version_code, version_code, MAX_CODE_LEN - 1);
    strncpy(verify->heartbeat_code, heartbeat_code, MAX_CODE_LEN - 1);
    strncpy(verify->appkey, appkey, MAX_APPKEY_LEN - 1);
    strncpy(verify->rsa_public_key, rsa_public_key, MAX_RSA_KEY_LEN - 1);
    verify->encode_type = 1; /* RSA模式 */
    verify->initialized = 1;
    return 0;
}

/**
 * 构建完整URL
 */
static void build_url(const T3Verify *verify, const char *code, char *url, int url_len) {
    if (verify->server_url[strlen(verify->server_url) - 1] == '/') {
        snprintf(url, url_len, "%s%s", verify->server_url, code);
    } else {
        snprintf(url, url_len, "%s/%s", verify->server_url, code);
    }
}

static int http_post(T3Verify *verify, const char *url, const char *post_data, char *response, int response_len) {
    URL_INFO original;
    char target[MAX_URL_LEN];
    int i;
    if (parse_url(url, &original) < 0) return -1;

    for (i = -1; i < verify->server_count; i++) {
        const char *base = i < 0 ? verify->server_url : verify->server_urls[i];
        size_t base_len;
        if (i >= 0 && strcmp(base, verify->server_url) == 0) continue;
        base_len = strlen(base);
        if (base_len > 0 && base[base_len - 1] == '/' && original.path[0] == '/')
            snprintf(target, sizeof(target), "%.*s%s", (int)base_len - 1, base, original.path);
        else
            snprintf(target, sizeof(target), "%s%s", base, original.path);
        if (http_post_once(target, post_data, response, response_len) == 0) {
            strncpy(verify->server_url, base, MAX_URL_LEN - 1);
            verify->server_url[MAX_URL_LEN - 1] = '\0';
            return 0;
        }
    }
    return -1;
}

/**
 * 统一编码: 根据 encode_type 选择 Base64+HEX 或 RSA+HEX
 * 返回 malloc 分配的字符串，调用者需 free
 */
static char* encode_value(const T3Verify *verify, const char *value) {
    if (verify->encode_type == 0) {
        int max_len = ((strlen(value) + 2) / 3) * 4 * 2 + 64;
        char *hex = (char*)malloc(max_len);
        if (!hex) return NULL;
        if (base64_encode_to_hex(value, verify->base64_charset, hex, max_len) < 0) {
            free(hex); return NULL;
        }
        return hex;
    } else {
        return rsa_encrypt_hex(value, verify->rsa_public_key);
    }
}

/**
 * 编码参数（统一版，支持 Base64 和 RSA）
 * 关键: 每个值只编码一次，RSA有随机填充
 */
static int encode_params(const T3Verify *verify, const char **keys, const char **values,
                        int param_count, char *post_data, int post_data_len, 
                        char *s_original, int s_orig_len) {
    char s_value[33];
    char *enc[16];
    char *s_enc;
    char parts[4096];
    int i, j;
    
    /* 1. 每个值只编码一次 */
    for (i = 0; i < param_count && i < 16; i++) {
        enc[i] = encode_value(verify, values[i]);
        if (!enc[i]) {
            for (j = 0; j < i; j++) free(enc[j]);
            return -1;
        }
    }
    
    /* 2. 用已编码的值构建签名字符串 */
    parts[0] = '\0';
    for (i = 0; i < param_count; i++) {
        if (i > 0) strcat(parts, "&");
        strcat(parts, keys[i]);
        strcat(parts, "=");
        strcat(parts, enc[i]);
    }
    snprintf(s_original, s_orig_len, "%s&%s", parts, verify->appkey);
    md5_string(s_original, s_value);
    
    /* 3. 构建POST数据（复用已编码的值） */
    post_data[0] = '\0';
    for (i = 0; i < param_count; i++) {
        if (i > 0) strcat(post_data, "&");
        strcat(post_data, keys[i]);
        strcat(post_data, "=");
        strcat(post_data, enc[i]);
    }
    for (i = 0; i < param_count; i++) free(enc[i]);
    
    /* 4. 编码并添加s参数 */
    s_enc = encode_value(verify, s_value);
    if (!s_enc) return -1;
    strcat(post_data, "&s=");
    strcat(post_data, s_enc);
    free(s_enc);
    
    return 0;
}

/**
 * 解码响应（统一版，支持 Base64 和 RSA）
 */
static int decode_response(const T3Verify *verify, const char *response, 
                          char *decoded, int decoded_len) {
    const char *data = response;
    const char *newline;
    char *clean, *s, *d;
    size_t clen;
    int result;
    
    newline = strchr(response, '\n');
    if (newline) data = newline + 1;
    
    clean = strdup(data);
    if (!clean) return -1;
    s = clean; d = clean;
    while (*s) { if (*s != '\r' && *s != '\n') *d++ = *s; s++; }
    *d = '\0';
    
    clen = strlen(clean);
    if (clen > 0 && clean[clen - 1] == '0') clean[clen - 1] = '\0';
    
    if (verify->encode_type == 0) {
        /* Base64模式 */
        result = base64_decode_custom(clean, verify->base64_charset, 
                                     (unsigned char *)decoded, decoded_len);
        if (result >= 0 && result < decoded_len) decoded[result] = '\0';
    } else {
        /* RSA模式: Base64解码 → 公钥解密 */
        char *dec = rsa_decrypt_b64(clean, verify->rsa_public_key);
        if (dec) {
            int len = strlen(dec);
            if (len < decoded_len) { strcpy(decoded, dec); result = len; }
            else { strncpy(decoded, dec, decoded_len-1); decoded[decoded_len-1] = '\0'; result = decoded_len-1; }
            free(dec);
        } else {
            result = -1;
        }
    }
    
    free(clean);
    return result;
}

/**
 * 单码卡密登录
 */
int t3verify_login(T3Verify *verify, const char *kami, const char *imei, T3LoginResult *result) {
    char url[MAX_URL_LEN];
    char post_data[2048];
    char response[MAX_RESPONSE_LEN];
    char decoded[MAX_RESPONSE_LEN];
    char s_original[1024];
    char t_str[32];
    const char *keys[3] = {"kami", "imei", "t"};
    const char *values[3];
    time_t current_time;
    int response_time, time_diff, code;
    char token[MAX_TOKEN_LEN], expected_token[33];
    char kami_id[MAX_TOKEN_LEN], end_time[MAX_END_TIME_LEN], statecode[MAX_STATECODE_LEN];
    char date_str[16];
    char token_source[4096];
    struct tm *tm_info;
    
    memset(result, 0, sizeof(T3LoginResult));
    
    /* 检查初始化 */
    if (!verify->initialized) {
        strcpy(result->error, "未初始化，请先调用 t3verify_init()");
        return -1;
    }
    
    /* 获取当前时间戳 */
    current_time = time(NULL);
    snprintf(t_str, sizeof(t_str), "%ld", (long)current_time);
    
    /* 构建URL */
    build_url(verify, verify->login_code, url, sizeof(url));
    
    /* 编码参数 */
    values[0] = kami;
    values[1] = imei;
    values[2] = t_str;
    
    if (encode_params(verify, keys, values, 3, post_data, sizeof(post_data), 
                     s_original, sizeof(s_original)) < 0) {
        strcpy(result->error, "参数编码失败");
        return -1;
    }
    
    /* 发送HTTP请求 */
    if (http_post(verify, url, post_data, response, sizeof(response)) < 0) {
        strcpy(result->error, ALL_SERVERS_UNAVAILABLE);
        return -1;
    }
    
    /* 解码响应 */
    int decoded_len = decode_response(verify, response, decoded, sizeof(decoded));
    if (decoded_len < 0) {
        strcpy(result->error, "响应解码失败");
        return -1;
    }
    
    /* 解析JSON */
    if (json_get_int(decoded, "code", &code) < 0) {
        char dbg[256];
        snprintf(dbg, sizeof(dbg), "响应不是有效的JSON格式(原始: %.100s)", decoded);
        strncpy(result->error, dbg, MAX_ERROR_LEN - 1);
        result->error[MAX_ERROR_LEN - 1] = '\0';
        return -1;
    }
    
    if (code != 200) {
        char msg[MAX_ERROR_LEN];
        if (json_get_string(decoded, "msg", msg, sizeof(msg)) == 0) {
            strncpy(result->error, msg, MAX_ERROR_LEN - 1);
        } else {
            strcpy(result->error, "未知错误");
        }
        return -1;
    }
    
    /* 提取必要字段 */
    if (json_get_string(decoded, "id", kami_id, sizeof(kami_id)) < 0 ||
        json_get_string(decoded, "end_time", end_time, sizeof(end_time)) < 0 ||
        json_get_string(decoded, "token", token, sizeof(token)) < 0 ||
        json_get_string(decoded, "statecode", statecode, sizeof(statecode)) < 0 ||
        json_get_int(decoded, "time", &response_time) < 0) {
        strcpy(result->error, "响应数据缺少必要字段");
        return -1;
    }
    
    /* 时间戳校验 */
    time_diff = abs((int)current_time - response_time);
    if (time_diff > 5) {
        snprintf(result->error, MAX_ERROR_LEN, "时间戳校验失败，相差%d秒", time_diff);
        return -1;
    }
    
    /* 生成预期token */
    tm_info = localtime(&current_time);
    strftime(date_str, sizeof(date_str), "%Y%m%d%H%M", tm_info);
    snprintf(token_source, sizeof(token_source), "%s%s%s%s%s", 
            kami_id, verify->appkey, s_original, end_time, date_str);
    md5_string(token_source, expected_token);
    
    /* Token校验 */
    if (strcasecmp(token, expected_token) != 0) {
        strcpy(result->error, "token校验失败");
        return -1;
    }
    
    /* 保存状态 */
    strncpy(verify->statecode, statecode, MAX_STATECODE_LEN - 1);
    strncpy(verify->end_time, end_time, MAX_END_TIME_LEN - 1);
    
    /* 设置返回结果 */
    result->success = 1;
    strncpy(result->id, kami_id, MAX_TOKEN_LEN - 1);
    strncpy(result->end_time, end_time, MAX_END_TIME_LEN - 1);
    strncpy(result->statecode, statecode, MAX_STATECODE_LEN - 1);
    json_get_string(decoded, "recharge", result->recharge, sizeof(result->recharge));
    json_get_string(decoded, "use_time", result->use_time, sizeof(result->use_time));
    json_get_string(decoded, "available", result->available, sizeof(result->available));
    json_get_string(decoded, "imei", result->imei, sizeof(result->imei));
    json_get_string(decoded, "change", result->change, sizeof(result->change));
    json_get_string(decoded, "core", result->core, sizeof(result->core));
    json_get_string(decoded, "amount", result->amount, sizeof(result->amount));
    
    return 0;
}

/**
 * 获取公告内容
 */
int t3verify_get_notice(T3Verify *verify, T3NoticeResult *result) {
    char url[MAX_URL_LEN];
    char post_data[2048];
    char response[MAX_RESPONSE_LEN];
    char decoded[MAX_RESPONSE_LEN];
    char s_original[1024];
    char t_str[32];
    const char *keys[1] = {"t"};
    const char *values[1];
    time_t current_time;
    int code;
    char msg[MAX_NOTICE_LEN];
    
    memset(result, 0, sizeof(T3NoticeResult));
    
    /* 检查初始化 */
    if (!verify->initialized) {
        strcpy(result->error, "未初始化，请先调用 t3verify_init()");
        return -1;
    }
    
    /* 获取当前时间戳 */
    current_time = time(NULL);
    snprintf(t_str, sizeof(t_str), "%ld", (long)current_time);
    
    /* 构建URL */
    build_url(verify, verify->notice_code, url, sizeof(url));
    
    /* 编码参数 */
    values[0] = t_str;
    
    if (encode_params(verify, keys, values, 1, post_data, sizeof(post_data),
                     s_original, sizeof(s_original)) < 0) {
        strcpy(result->error, "参数编码失败");
        return -1;
    }
    
    /* 发送HTTP请求 */
    if (http_post(verify, url, post_data, response, sizeof(response)) < 0) {
        strcpy(result->error, ALL_SERVERS_UNAVAILABLE);
        return -1;
    }
    
    /* 解码响应 */
    if (decode_response(verify, response, decoded, sizeof(decoded)) < 0) {
        strcpy(result->error, "响应解码失败");
        return -1;
    }
    
    /* 解析JSON */
    if (json_get_int(decoded, "code", &code) < 0) {
        char dbg[256];
        snprintf(dbg, sizeof(dbg), "响应不是有效的JSON格式(原始: %.100s)", decoded);
        strncpy(result->error, dbg, MAX_ERROR_LEN - 1);
        result->error[MAX_ERROR_LEN - 1] = '\0';
        return -1;
    }
    
    if (code != 200) {
        if (json_get_string(decoded, "msg", msg, sizeof(msg)) == 0) {
            strncpy(result->error, msg, MAX_ERROR_LEN - 1);
        } else {
            strcpy(result->error, "未知错误");
        }
        return -1;
    }
    
    /* 提取公告内容 */
    if (json_get_string(decoded, "msg", msg, sizeof(msg)) == 0) {
        strncpy(result->notice, msg, MAX_NOTICE_LEN - 1);
    }
    
    result->success = 1;
    return 0;
}

/**
 * 获取最新版本号
 */
int t3verify_get_latest_version(T3Verify *verify, T3VersionResult *result) {
    char url[MAX_URL_LEN];
    char post_data[2048];
    char response[MAX_RESPONSE_LEN];
    char decoded[MAX_RESPONSE_LEN];
    char s_original[1024];
    char t_str[32];
    const char *keys[1] = {"t"};
    const char *values[1];
    time_t current_time;
    int code;
    char msg[MAX_VERSION_LEN];
    
    memset(result, 0, sizeof(T3VersionResult));
    
    /* 检查初始化 */
    if (!verify->initialized) {
        strcpy(result->error, "未初始化，请先调用 t3verify_init()");
        return -1;
    }
    
    /* 获取当前时间戳 */
    current_time = time(NULL);
    snprintf(t_str, sizeof(t_str), "%ld", (long)current_time);
    
    /* 构建URL */
    build_url(verify, verify->version_code, url, sizeof(url));
    
    /* 编码参数 */
    values[0] = t_str;
    
    if (encode_params(verify, keys, values, 1, post_data, sizeof(post_data),
                     s_original, sizeof(s_original)) < 0) {
        strcpy(result->error, "参数编码失败");
        return -1;
    }
    
    /* 发送HTTP请求 */
    if (http_post(verify, url, post_data, response, sizeof(response)) < 0) {
        strcpy(result->error, ALL_SERVERS_UNAVAILABLE);
        return -1;
    }
    
    /* 解码响应 */
    if (decode_response(verify, response, decoded, sizeof(decoded)) < 0) {
        strcpy(result->error, "响应解码失败");
        return -1;
    }
    
    /* 解析JSON */
    if (json_get_int(decoded, "code", &code) < 0) {
        char dbg[256];
        snprintf(dbg, sizeof(dbg), "响应不是有效的JSON格式(原始: %.100s)", decoded);
        strncpy(result->error, dbg, MAX_ERROR_LEN - 1);
        result->error[MAX_ERROR_LEN - 1] = '\0';
        return -1;
    }
    
    if (code != 200) {
        if (json_get_string(decoded, "msg", msg, sizeof(msg)) == 0) {
            strncpy(result->error, msg, MAX_ERROR_LEN - 1);
        } else {
            strcpy(result->error, "未知错误");
        }
        return -1;
    }
    
    /* 提取版本号 */
    if (json_get_string(decoded, "msg", msg, sizeof(msg)) == 0) {
        strncpy(result->version, msg, MAX_VERSION_LEN - 1);
    }
    
    result->success = 1;
    return 0;
}

/**
 * 心跳验证
 */
int t3verify_heartbeat(T3Verify *verify, const char *kami, const char *statecode, T3Result *result) {
    char url[MAX_URL_LEN];
    char post_data[2048];
    char response[MAX_RESPONSE_LEN];
    char decoded[MAX_RESPONSE_LEN];
    char s_original[1024];
    char t_str[32];
    const char *keys[3] = {"kami", "statecode", "t"};
    const char *values[3];
    time_t current_time;
    int code;
    char msg[MAX_ERROR_LEN];
    
    memset(result, 0, sizeof(T3Result));
    
    if (!verify->initialized) {
        strcpy(result->error, "未初始化");
        return -1;
    }
    
    current_time = time(NULL);
    snprintf(t_str, sizeof(t_str), "%ld", (long)current_time);
    build_url(verify, verify->heartbeat_code, url, sizeof(url));
    
    values[0] = kami;
    values[1] = statecode;
    values[2] = t_str;
    
    if (encode_params(verify, keys, values, 3, post_data, sizeof(post_data),
                     s_original, sizeof(s_original)) < 0) {
        strcpy(result->error, "参数编码失败");
        return -1;
    }
    
    if (http_post(verify, url, post_data, response, sizeof(response)) < 0) {
        strcpy(result->error, ALL_SERVERS_UNAVAILABLE);
        return -1;
    }
    
    if (decode_response(verify, response, decoded, sizeof(decoded)) < 0) {
        strcpy(result->error, "响应解码失败");
        return -1;
    }
    
    if (json_get_int(decoded, "code", &code) < 0) {
        strcpy(result->error, "响应不是有效的JSON格式");
        return -1;
    }
    
    if (code != 200) {
        if (json_get_string(decoded, "msg", msg, sizeof(msg)) == 0) {
            strncpy(result->error, msg, MAX_ERROR_LEN - 1);
        } else {
            strcpy(result->error, "未知错误");
        }
        return -1;
    }
    
    result->success = 1;
    return 0;
}

/* ========== 设置调用码辅助函数 ========== */

void t3verify_set_code(T3Verify *verify, const char *field, const char *code) {
    if (!verify || !field || !code) return;
    #define SET_IF(name, member) if (strcmp(field, name) == 0) { strncpy(verify->member, code, MAX_CODE_LEN-1); return; }
    SET_IF("query", query_code)
    SET_IF("register", register_code)
    SET_IF("user_login", user_login_code)
    SET_IF("user_heartbeat", user_heartbeat_code)
    SET_IF("qq_login", qq_login_code)
    SET_IF("bind_qq", bind_qq_code)
    SET_IF("change_password", change_password_code)
    SET_IF("user_cancel", user_cancel_code)
    SET_IF("recharge", recharge_code)
    SET_IF("kami_recharge", kami_recharge_code)
    SET_IF("unbind", unbind_code)
    SET_IF("ip_unbind", ip_unbind_code)
    SET_IF("disable", disable_code)
    SET_IF("check_update", check_update_code)
    SET_IF("get_variable", get_variable_code)
    SET_IF("modify_variable", modify_variable_code)
    SET_IF("modify_core", modify_core_code)
    SET_IF("get_kami_core", get_kami_core_code)
    SET_IF("get_user_core", get_user_core_code)
    SET_IF("online_kami", online_kami_code)
    SET_IF("online_user", online_user_code)
    SET_IF("cloud_doc", cloud_doc_code)
    SET_IF("app_sign", app_sign_code)
    SET_IF("qq_heartbeat", qq_heartbeat_code)
    SET_IF("qq_get_variable", get_variable_by_qq_code)
    SET_IF("qq_get_core", get_user_core_by_qq_code)
    SET_IF("qq_modify_core", modify_user_core_by_qq_code)
    SET_IF("qq_unbind", unbind_qq_code)
    SET_IF("heartbeat_any", heartbeat_any_code)
    #undef SET_IF
}

/* ========== 通用简单请求 ========== */

static int simple_request(T3Verify *verify, const char *code, const char *code_name,
                          const char **keys, const char **values, int param_count,
                          T3Result *result) {
    char url[MAX_URL_LEN];
    char post_data[4096];
    char response[MAX_RESPONSE_LEN];
    char decoded[MAX_RESPONSE_LEN];
    char s_original[2048];
    char t_str[32];
    int code_val;
    char msg[MAX_MSG_LEN];
    
    memset(result, 0, sizeof(T3Result));
    
    if (!verify->initialized) { strcpy(result->error, "未初始化"); return -1; }
    if (!code || strlen(code) == 0) {
        snprintf(result->error, MAX_ERROR_LEN, "未设置 %s 调用码", code_name);
        return -1;
    }
    
    /* 添加时间戳参数到末尾 */
    snprintf(t_str, sizeof(t_str), "%ld", (long)time(NULL));
    /* 复制keys和values，追加t参数 */
    const char *all_keys[16];
    const char *all_values[16];
    int total = param_count;
    int i;
    for (i = 0; i < param_count && i < 15; i++) { all_keys[i] = keys[i]; all_values[i] = values[i]; }
    all_keys[total] = "t"; all_values[total] = t_str; total++;
    
    build_url(verify, code, url, sizeof(url));
    
    if (encode_params(verify, all_keys, all_values, total, post_data, sizeof(post_data),
                     s_original, sizeof(s_original)) < 0) {
        strcpy(result->error, "参数编码失败"); return -1;
    }
    if (http_post(verify, url, post_data, response, sizeof(response)) < 0) {
        strcpy(result->error, ALL_SERVERS_UNAVAILABLE); return -1;
    }
    if (decode_response(verify, response, decoded, sizeof(decoded)) < 0) {
        strcpy(result->error, "响应解码失败"); return -1;
    }
    if (json_get_int(decoded, "code", &code_val) < 0) {
        char dbg[256];
        snprintf(dbg, sizeof(dbg), "响应不是有效的JSON格式(原始: %.100s)", decoded);
        strncpy(result->error, dbg, MAX_ERROR_LEN - 1);
        result->error[MAX_ERROR_LEN - 1] = '\0';
        return -1;
    }
    if (code_val != 200) {
        if (json_get_string(decoded, "msg", msg, sizeof(msg)) == 0) strncpy(result->error, msg, MAX_ERROR_LEN-1);
        else strcpy(result->error, "未知错误");
        return -1;
    }
    if (json_get_string(decoded, "msg", result->msg, MAX_MSG_LEN) < 0) result->msg[0] = '\0';
    result->success = 1;
    return 0;
}

/* ========== 查询卡密 ========== */

int t3verify_query_kami(T3Verify *verify, const char *kami, T3QueryResult *result) {
    char url[MAX_URL_LEN], post_data[4096], response[MAX_RESPONSE_LEN], decoded[MAX_RESPONSE_LEN], s_original[2048], t_str[32];
    int code_val;
    memset(result, 0, sizeof(T3QueryResult));
    if (!verify->initialized) { strcpy(result->error, "未初始化"); return -1; }
    if (strlen(verify->query_code) == 0) { strcpy(result->error, "未设置查询卡密调用码"); return -1; }
    snprintf(t_str, sizeof(t_str), "%ld", (long)time(NULL));
    const char *keys[2] = {"kami", "t"}; const char *values[2] = {kami, t_str};
    build_url(verify, verify->query_code, url, sizeof(url));
    if (encode_params(verify, keys, values, 2, post_data, sizeof(post_data), s_original, sizeof(s_original)) < 0) { strcpy(result->error, "参数编码失败"); return -1; }
    if (http_post(verify, url, post_data, response, sizeof(response)) < 0) { strcpy(result->error, ALL_SERVERS_UNAVAILABLE); return -1; }
    if (decode_response(verify, response, decoded, sizeof(decoded)) < 0) { strcpy(result->error, "响应解码失败"); return -1; }
    if (json_get_int(decoded, "code", &code_val) < 0) { char dbg[256]; snprintf(dbg, sizeof(dbg), "响应不是有效的JSON格式(原始: %.100s)", decoded); strncpy(result->error, dbg, MAX_ERROR_LEN-1); result->error[MAX_ERROR_LEN-1]='\0'; return -1; }
    if (code_val != 200) { char msg[MAX_ERROR_LEN]; if (json_get_string(decoded, "msg", msg, sizeof(msg))==0) strncpy(result->error, msg, MAX_ERROR_LEN-1); else strcpy(result->error, "未知错误"); return -1; }
    result->success = 1;
    json_get_string(decoded, "state", result->state, sizeof(result->state));
    json_get_string(decoded, "use", result->use, sizeof(result->use));
    json_get_string(decoded, "id", result->id, sizeof(result->id));
    json_get_string(decoded, "use_time", result->use_time, sizeof(result->use_time));
    json_get_string(decoded, "end_time", result->end_time, sizeof(result->end_time));
    json_get_string(decoded, "line_time", result->line_time, sizeof(result->line_time));
    json_get_string(decoded, "line", result->line, sizeof(result->line));
    json_get_string(decoded, "amount", result->amount, sizeof(result->amount));
    json_get_string(decoded, "available", result->available, sizeof(result->available));
    return 0;
}

/* ========== 检查更新 ========== */

int t3verify_check_update(T3Verify *verify, const char *ver, T3UpdateResult *result) {
    char url[MAX_URL_LEN], post_data[4096], response[MAX_RESPONSE_LEN], decoded[MAX_RESPONSE_LEN], s_original[2048], t_str[32];
    int code_val;
    memset(result, 0, sizeof(T3UpdateResult));
    if (!verify->initialized) { strcpy(result->error, "未初始化"); return -1; }
    if (strlen(verify->check_update_code) == 0) { strcpy(result->error, "未设置检查更新调用码"); return -1; }
    snprintf(t_str, sizeof(t_str), "%ld", (long)time(NULL));
    const char *keys[2] = {"ver", "t"}; const char *values[2] = {ver, t_str};
    build_url(verify, verify->check_update_code, url, sizeof(url));
    if (encode_params(verify, keys, values, 2, post_data, sizeof(post_data), s_original, sizeof(s_original)) < 0) { strcpy(result->error, "参数编码失败"); return -1; }
    if (http_post(verify, url, post_data, response, sizeof(response)) < 0) { strcpy(result->error, ALL_SERVERS_UNAVAILABLE); return -1; }
    if (decode_response(verify, response, decoded, sizeof(decoded)) < 0) { strcpy(result->error, "响应解码失败"); return -1; }
    if (json_get_int(decoded, "code", &code_val) < 0) { char dbg[256]; snprintf(dbg, sizeof(dbg), "响应不是有效的JSON格式(原始: %.100s)", decoded); strncpy(result->error, dbg, MAX_ERROR_LEN-1); result->error[MAX_ERROR_LEN-1]='\0'; return -1; }
    if (code_val == 200) {
        result->success = 1; result->has_update = 1;
        json_get_string(decoded, "ver", result->ver, sizeof(result->ver));
        json_get_string(decoded, "version", result->version, sizeof(result->version));
        json_get_string(decoded, "uplog", result->uplog, sizeof(result->uplog));
        json_get_string(decoded, "upurl", result->upurl, sizeof(result->upurl));
        return 0;
    } else if (code_val == 201) {
        result->success = 1; result->has_update = 0;
        json_get_string(decoded, "msg", result->msg, sizeof(result->msg));
        return 0;
    }
    char msg[MAX_ERROR_LEN]; if (json_get_string(decoded, "msg", msg, sizeof(msg))==0) strncpy(result->error, msg, MAX_ERROR_LEN-1); else strcpy(result->error, "未知错误");
    return -1;
}

/* ========== 云文档 ========== */

int t3verify_get_cloud_doc(T3Verify *verify, const char *token, T3CloudDocResult *result) {
    T3Result r; const char *keys[1] = {"token"}; const char *values[1] = {token};
    memset(result, 0, sizeof(T3CloudDocResult));
    if (simple_request(verify, verify->cloud_doc_code, "云文档", keys, values, 1, &r) < 0) { strcpy(result->error, r.error); return -1; }
    result->success = 1; strncpy(result->content, r.msg, MAX_MSG_LEN-1);
    return 0;
}

/* ========== 应用签名 ========== */

int t3verify_app_sign(T3Verify *verify, const char *autograph, T3AppSignResult *result) {
    char url[MAX_URL_LEN], post_data[4096], response[MAX_RESPONSE_LEN], decoded[MAX_RESPONSE_LEN], s_original[2048], t_str[32];
    int code_val;
    memset(result, 0, sizeof(T3AppSignResult));
    if (!verify->initialized) { strcpy(result->error, "未初始化"); return -1; }
    if (strlen(verify->app_sign_code) == 0) { strcpy(result->error, "未设置应用签名调用码"); return -1; }
    snprintf(t_str, sizeof(t_str), "%ld", (long)time(NULL));
    const char *keys[2] = {"autograph", "t"}; const char *values[2] = {autograph, t_str};
    build_url(verify, verify->app_sign_code, url, sizeof(url));
    if (encode_params(verify, keys, values, 2, post_data, sizeof(post_data), s_original, sizeof(s_original)) < 0) { strcpy(result->error, "参数编码失败"); return -1; }
    if (http_post(verify, url, post_data, response, sizeof(response)) < 0) { strcpy(result->error, ALL_SERVERS_UNAVAILABLE); return -1; }
    if (decode_response(verify, response, decoded, sizeof(decoded)) < 0) { strcpy(result->error, "响应解码失败"); return -1; }
    if (json_get_int(decoded, "code", &code_val) < 0) { char dbg[256]; snprintf(dbg, sizeof(dbg), "响应不是有效的JSON格式(原始: %.100s)", decoded); strncpy(result->error, dbg, MAX_ERROR_LEN-1); result->error[MAX_ERROR_LEN-1]='\0'; return -1; }
    if (code_val != 200) { char msg[MAX_ERROR_LEN]; if (json_get_string(decoded, "msg", msg, sizeof(msg))==0) strncpy(result->error, msg, MAX_ERROR_LEN-1); else strcpy(result->error, "未知错误"); return -1; }
    result->success = 1;
    json_get_string(decoded, "msg", result->msg, sizeof(result->msg));
    json_get_string(decoded, "autograph", result->autograph, sizeof(result->autograph));
    json_get_int(decoded, "time", (int*)&result->time);
    return 0;
}

/* ========== 用户体系 ========== */

int t3verify_user_register(T3Verify *verify, const char *user, const char *pass, const char *email, T3Result *result) {
    if (email && strlen(email) > 0) {
        const char *keys[3] = {"user", "pass", "email"}; const char *values[3] = {user, pass, email};
        return simple_request(verify, verify->register_code, "用户注册", keys, values, 3, result);
    } else {
        const char *keys[2] = {"user", "pass"}; const char *values[2] = {user, pass};
        return simple_request(verify, verify->register_code, "用户注册", keys, values, 2, result);
    }
}

int t3verify_user_login(T3Verify *verify, const char *user, const char *pass, const char *imei, T3LoginResult *result) {
    char url[MAX_URL_LEN], post_data[4096], response[MAX_RESPONSE_LEN], decoded[MAX_RESPONSE_LEN], s_original[2048], t_str[32];
    int code_val;
    memset(result, 0, sizeof(T3LoginResult));
    if (!verify->initialized) { strcpy(result->error, "未初始化"); return -1; }
    if (strlen(verify->user_login_code) == 0) { strcpy(result->error, "未设置用户登录调用码"); return -1; }
    snprintf(t_str, sizeof(t_str), "%ld", (long)time(NULL));
    const char *keys[4] = {"user", "pass", "imei", "t"}; const char *values[4] = {user, pass, imei, t_str};
    build_url(verify, verify->user_login_code, url, sizeof(url));
    if (encode_params(verify, keys, values, 4, post_data, sizeof(post_data), s_original, sizeof(s_original)) < 0) { strcpy(result->error, "参数编码失败"); return -1; }
    if (http_post(verify, url, post_data, response, sizeof(response)) < 0) { strcpy(result->error, ALL_SERVERS_UNAVAILABLE); return -1; }
    if (decode_response(verify, response, decoded, sizeof(decoded)) < 0) { strcpy(result->error, "响应解码失败"); return -1; }
    if (json_get_int(decoded, "code", &code_val) < 0) { char dbg[256]; snprintf(dbg, sizeof(dbg), "响应不是有效的JSON格式(原始: %.100s)", decoded); strncpy(result->error, dbg, MAX_ERROR_LEN-1); result->error[MAX_ERROR_LEN-1]='\0'; return -1; }
    if (code_val != 200) { char msg[MAX_ERROR_LEN]; if (json_get_string(decoded, "msg", msg, sizeof(msg))==0) strncpy(result->error, msg, MAX_ERROR_LEN-1); else strcpy(result->error, "未知错误"); return -1; }
    result->success = 1;
    json_get_string(decoded, "id", result->id, sizeof(result->id));
    json_get_string(decoded, "end_time", result->end_time, sizeof(result->end_time));
    json_get_string(decoded, "statecode", result->statecode, sizeof(result->statecode));
    json_get_string(decoded, "recharge", result->recharge, sizeof(result->recharge));
    json_get_string(decoded, "use_time", result->use_time, sizeof(result->use_time));
    json_get_string(decoded, "available", result->available, sizeof(result->available));
    json_get_string(decoded, "imei", result->imei, sizeof(result->imei));
    json_get_string(decoded, "change", result->change, sizeof(result->change));
    json_get_string(decoded, "core", result->core, sizeof(result->core));
    strncpy(verify->statecode, result->statecode, MAX_STATECODE_LEN-1);
    strncpy(verify->end_time, result->end_time, MAX_END_TIME_LEN-1);
    return 0;
}

int t3verify_user_heartbeat(T3Verify *verify, const char *user, const char *pass, const char *statecode, T3Result *result) {
    const char *keys[3] = {"user", "pass", "statecode"}; const char *values[3] = {user, pass, statecode};
    return simple_request(verify, verify->user_heartbeat_code, "用户心跳", keys, values, 3, result);
}

int t3verify_qq_login(T3Verify *verify, const char *openid, const char *access_token, T3LoginResult *result) {
    char url[MAX_URL_LEN], post_data[4096], response[MAX_RESPONSE_LEN], decoded[MAX_RESPONSE_LEN], s_original[2048], t_str[32];
    int code_val;
    memset(result, 0, sizeof(T3LoginResult));
    if (!verify->initialized) { strcpy(result->error, "未初始化"); return -1; }
    if (strlen(verify->qq_login_code) == 0) { strcpy(result->error, "未设置QQ登录调用码"); return -1; }
    snprintf(t_str, sizeof(t_str), "%ld", (long)time(NULL));
    const char *keys[3] = {"openid", "access_token", "t"}; const char *values[3] = {openid, access_token, t_str};
    build_url(verify, verify->qq_login_code, url, sizeof(url));
    if (encode_params(verify, keys, values, 3, post_data, sizeof(post_data), s_original, sizeof(s_original)) < 0) { strcpy(result->error, "参数编码失败"); return -1; }
    if (http_post(verify, url, post_data, response, sizeof(response)) < 0) { strcpy(result->error, ALL_SERVERS_UNAVAILABLE); return -1; }
    if (decode_response(verify, response, decoded, sizeof(decoded)) < 0) { strcpy(result->error, "响应解码失败"); return -1; }
    if (json_get_int(decoded, "code", &code_val) < 0) { char dbg[256]; snprintf(dbg, sizeof(dbg), "响应不是有效的JSON格式(原始: %.100s)", decoded); strncpy(result->error, dbg, MAX_ERROR_LEN-1); result->error[MAX_ERROR_LEN-1]='\0'; return -1; }
    if (code_val != 200) { char msg[MAX_ERROR_LEN]; if (json_get_string(decoded, "msg", msg, sizeof(msg))==0) strncpy(result->error, msg, MAX_ERROR_LEN-1); else strcpy(result->error, "未知错误"); return -1; }
    result->success = 1;
    json_get_string(decoded, "id", result->id, sizeof(result->id));
    json_get_string(decoded, "end_time", result->end_time, sizeof(result->end_time));
    json_get_string(decoded, "statecode", result->statecode, sizeof(result->statecode));
    json_get_string(decoded, "recharge", result->recharge, sizeof(result->recharge));
    json_get_string(decoded, "use_time", result->use_time, sizeof(result->use_time));
    json_get_string(decoded, "available", result->available, sizeof(result->available));
    json_get_string(decoded, "imei", result->imei, sizeof(result->imei));
    json_get_string(decoded, "change", result->change, sizeof(result->change));
    json_get_string(decoded, "core", result->core, sizeof(result->core));
    strncpy(verify->statecode, result->statecode, MAX_STATECODE_LEN-1);
    strncpy(verify->end_time, result->end_time, MAX_END_TIME_LEN-1);
    return 0;
}

int t3verify_bind_qq(T3Verify *verify, const char *user, const char *pass, const char *openid, const char *access_token, T3Result *result) {
    const char *keys[4] = {"user", "pass", "openid", "access_token"}; const char *values[4] = {user, pass, openid, access_token};
    return simple_request(verify, verify->bind_qq_code, "绑定QQ", keys, values, 4, result);
}

int t3verify_change_password(T3Verify *verify, const char *user, const char *oldpass, const char *newpass, T3Result *result) {
    const char *keys[3] = {"user", "oldpass", "newpass"}; const char *values[3] = {user, oldpass, newpass};
    return simple_request(verify, verify->change_password_code, "修改密码", keys, values, 3, result);
}

int t3verify_user_cancel(T3Verify *verify, const char *user, const char *pass, T3Result *result) {
    const char *keys[2] = {"user", "pass"}; const char *values[2] = {user, pass};
    return simple_request(verify, verify->user_cancel_code, "用户注销", keys, values, 2, result);
}

int t3verify_recharge(T3Verify *verify, const char *user, const char *card, T3Result *result) {
    const char *keys[2] = {"user", "card"}; const char *values[2] = {user, card};
    return simple_request(verify, verify->recharge_code, "用户充值", keys, values, 2, result);
}

int t3verify_kami_recharge(T3Verify *verify, const char *target_kami, const char *source_kami, T3Result *result) {
    const char *keys[2] = {"target_kami", "source_kami"}; const char *values[2] = {target_kami, source_kami};
    return simple_request(verify, verify->kami_recharge_code, "单码以卡充卡", keys, values, 2, result);
}

/* ========== 设备与安全 ========== */

int t3verify_unbind_kami(T3Verify *verify, const char *kami, const char *imei, T3Result *result) {
    const char *keys[2] = {"kami", "imei"}; const char *values[2] = {kami, imei};
    return simple_request(verify, verify->unbind_code, "解绑设备", keys, values, 2, result);
}

int t3verify_unbind_user(T3Verify *verify, const char *user, const char *pass, const char *imei, T3Result *result) {
    const char *keys[3] = {"user", "pass", "imei"}; const char *values[3] = {user, pass, imei};
    return simple_request(verify, verify->unbind_code, "解绑设备", keys, values, 3, result);
}

int t3verify_ip_unbind_kami(T3Verify *verify, const char *kami, T3Result *result) {
    const char *keys[1] = {"kami"}; const char *values[1] = {kami};
    return simple_request(verify, verify->ip_unbind_code, "IP解绑", keys, values, 1, result);
}

int t3verify_ip_unbind_user(T3Verify *verify, const char *user, const char *pass, T3Result *result) {
    const char *keys[2] = {"user", "pass"}; const char *values[2] = {user, pass};
    return simple_request(verify, verify->ip_unbind_code, "IP解绑", keys, values, 2, result);
}

int t3verify_disable_kami(T3Verify *verify, const char *kami, T3Result *result) {
    const char *keys[1] = {"kami"}; const char *values[1] = {kami};
    return simple_request(verify, verify->disable_code, "禁用", keys, values, 1, result);
}

int t3verify_disable_user(T3Verify *verify, const char *user, const char *pass, T3Result *result) {
    const char *keys[2] = {"user", "pass"}; const char *values[2] = {user, pass};
    return simple_request(verify, verify->disable_code, "禁用", keys, values, 2, result);
}

/* ========== 远程变量 ========== */

int t3verify_get_variable_by_kami(T3Verify *verify, const char *kami, const char *valueid, const char *valuename, T3VariableResult *result) {
    T3Result r; const char *keys[3] = {"kami", "valueid", "valuename"}; const char *values[3] = {kami, valueid, valuename};
    memset(result, 0, sizeof(T3VariableResult));
    if (simple_request(verify, verify->get_variable_code, "获取变量", keys, values, 3, &r) < 0) { strcpy(result->error, r.error); return -1; }
    result->success = 1; strncpy(result->value, r.msg, MAX_VALUE_LEN-1); return 0;
}

int t3verify_get_variable_by_user(T3Verify *verify, const char *user, const char *pass, const char *valueid, const char *valuename, T3VariableResult *result) {
    T3Result r; const char *keys[4] = {"user", "pass", "valueid", "valuename"}; const char *values[4] = {user, pass, valueid, valuename};
    memset(result, 0, sizeof(T3VariableResult));
    if (simple_request(verify, verify->get_variable_code, "获取变量", keys, values, 4, &r) < 0) { strcpy(result->error, r.error); return -1; }
    result->success = 1; strncpy(result->value, r.msg, MAX_VALUE_LEN-1); return 0;
}

int t3verify_modify_variable_by_kami(T3Verify *verify, const char *kami, const char *valueid, const char *valuecontent, T3Result *result) {
    const char *keys[3] = {"kami", "valueid", "valuecontent"}; const char *values[3] = {kami, valueid, valuecontent};
    return simple_request(verify, verify->modify_variable_code, "修改变量", keys, values, 3, result);
}

int t3verify_modify_variable_by_user(T3Verify *verify, const char *user, const char *pass, const char *valueid, const char *valuecontent, T3Result *result) {
    const char *keys[4] = {"user", "pass", "valueid", "valuecontent"}; const char *values[4] = {user, pass, valueid, valuecontent};
    return simple_request(verify, verify->modify_variable_code, "修改变量", keys, values, 4, result);
}

/* ========== 核心数据 ========== */

int t3verify_modify_core_by_kami(T3Verify *verify, const char *kami, const char *core, T3Result *result) {
    const char *keys[2] = {"kami", "core"}; const char *values[2] = {kami, core};
    return simple_request(verify, verify->modify_core_code, "修改核心数据", keys, values, 2, result);
}

int t3verify_modify_core_by_user(T3Verify *verify, const char *user, const char *pass, const char *core, T3Result *result) {
    const char *keys[3] = {"user", "pass", "core"}; const char *values[3] = {user, pass, core};
    return simple_request(verify, verify->modify_core_code, "修改核心数据", keys, values, 3, result);
}

int t3verify_get_core_by_kami(T3Verify *verify, const char *kami, T3CoreResult *result) {
    T3Result r; const char *keys[1] = {"kami"}; const char *values[1] = {kami};
    memset(result, 0, sizeof(T3CoreResult));
    if (simple_request(verify, verify->get_kami_core_code, "获取卡密核心数据", keys, values, 1, &r) < 0) { strcpy(result->error, r.error); return -1; }
    result->success = 1; strncpy(result->core, r.msg, MAX_CORE_LEN-1); return 0;
}

int t3verify_get_core_by_user(T3Verify *verify, const char *user, const char *pass, T3CoreResult *result) {
    T3Result r; const char *keys[2] = {"user", "pass"}; const char *values[2] = {user, pass};
    memset(result, 0, sizeof(T3CoreResult));
    if (simple_request(verify, verify->get_user_core_code, "获取用户核心数据", keys, values, 2, &r) < 0) { strcpy(result->error, r.error); return -1; }
    result->success = 1; strncpy(result->core, r.msg, MAX_CORE_LEN-1); return 0;
}

/* ========== 在线数量 ========== */

int t3verify_get_online_kami_count(T3Verify *verify, T3OnlineResult *result) {
    T3Result r;
    memset(result, 0, sizeof(T3OnlineResult));
    if (simple_request(verify, verify->online_kami_code, "获取在线卡密数量", NULL, NULL, 0, &r) < 0) { strcpy(result->error, r.error); return -1; }
    result->success = 1; result->count = atoi(r.msg); return 0;
}

int t3verify_get_online_user_count(T3Verify *verify, T3OnlineResult *result) {
    T3Result r;
    memset(result, 0, sizeof(T3OnlineResult));
    if (simple_request(verify, verify->online_user_code, "获取在线用户数量", NULL, NULL, 0, &r) < 0) { strcpy(result->error, r.error); return -1; }
    result->success = 1; result->count = atoi(r.msg); return 0;
}

/* ========== QQ 凭证 / 统一心跳 ========== */

int t3verify_qq_heartbeat(T3Verify *verify, const char *openid, const char *access_token, const char *statecode, T3Result *result) {
    const char *keys[3] = {"openid", "access_token", "statecode"}; const char *values[3] = {openid, access_token, statecode};
    return simple_request(verify, verify->qq_heartbeat_code, "QQ心跳", keys, values, 3, result);
}

int t3verify_get_variable_by_qq(T3Verify *verify, const char *openid, const char *access_token, const char *valueid, const char *valuename, T3VariableResult *result) {
    T3Result r; const char *keys[4] = {"openid", "access_token", "valueid", "valuename"}; const char *values[4] = {openid, access_token, valueid, valuename};
    memset(result, 0, sizeof(T3VariableResult));
    if (simple_request(verify, verify->get_variable_by_qq_code, "QQ获取变量", keys, values, 4, &r) < 0) { strcpy(result->error, r.error); return -1; }
    result->success = 1; strncpy(result->value, r.msg, MAX_VALUE_LEN-1); return 0;
}

int t3verify_get_user_core_by_qq(T3Verify *verify, const char *openid, const char *access_token, T3CoreResult *result) {
    T3Result r; const char *keys[2] = {"openid", "access_token"}; const char *values[2] = {openid, access_token};
    memset(result, 0, sizeof(T3CoreResult));
    if (simple_request(verify, verify->get_user_core_by_qq_code, "QQ获取核心数据", keys, values, 2, &r) < 0) { strcpy(result->error, r.error); return -1; }
    result->success = 1; strncpy(result->core, r.msg, MAX_CORE_LEN-1); return 0;
}

int t3verify_modify_user_core_by_qq(T3Verify *verify, const char *openid, const char *access_token, const char *core, T3Result *result) {
    const char *keys[3] = {"openid", "access_token", "core"}; const char *values[3] = {openid, access_token, core};
    return simple_request(verify, verify->modify_user_core_by_qq_code, "QQ修改核心数据", keys, values, 3, result);
}

int t3verify_unbind_qq(T3Verify *verify, const char *user, const char *pass, T3Result *result) {
    const char *keys[2] = {"user", "pass"}; const char *values[2] = {user, pass};
    return simple_request(verify, verify->unbind_qq_code, "解绑QQ", keys, values, 2, result);
}

int t3verify_heartbeat_any(T3Verify *verify, const char *statecode, T3Result *result) {
    const char *keys[1] = {"statecode"}; const char *values[1] = {statecode};
    return simple_request(verify, verify->heartbeat_any_code, "统一心跳", keys, values, 1, result);
}
