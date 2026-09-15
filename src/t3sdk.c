/*
 * T3 网络验证 SDK 核心实现
 * 实现全部 33 个 WebAPI 接口的调用逻辑
 * 包含加密、签名、编码、HTTP 请求完整流程
 */
#include "t3sdk.h"
#include "md5.h"
#include "crypto.h"
#include "http.h"
#include "t3json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ==================== 内部结构 ==================== */
struct t3_verify {
    t3_config_t config;
    int initialized;
};

/* 接口路径定义 */
#define T3_API_KAMI_LOGIN       "/kami-login"
#define T3_API_KAMI_QUERY       "/query-kami"
#define T3_API_KAMI_CORE        "/get-kami-core"
#define T3_API_KAMI_ONLINE      "/online-kami"
#define T3_API_KAMI_HEARTBEAT   "/kami-heartbeat"

#define T3_API_UNBIND           "/unbind"
#define T3_API_IP_UNBIND        "/ip-unbind"
#define T3_API_DISABLE          "/disable"

#define T3_API_USER_REGISTER    "/user-register"
#define T3_API_USER_LOGIN       "/user-login"
#define T3_API_USER_HEARTBEAT   "/user-heartbeat"
#define T3_API_RECHARGE         "/recharge"
#define T3_API_CHANGE_PASS      "/change-password"
#define T3_API_USER_CANCEL      "/user-cancel"
#define T3_API_USER_CORE        "/get-user-core"
#define T3_API_USER_ONLINE      "/online-user"

#define T3_API_BIND_QQ          "/bind-qq"
#define T3_API_QQ_LOGIN         "/qq-login"
#define T3_API_QQ_HEARTBEAT     "/qq-heartbeat"
#define T3_API_QQ_GET_VAR       "/qq-get-variable"
#define T3_API_QQ_GET_CORE      "/qq-get-core"
#define T3_API_QQ_MODIFY_CORE   "/qq-modify-core"
#define T3_API_QQ_UNBIND        "/qq-unbind"

#define T3_API_HEARTBEAT_ANY    "/heartbeat-any"

#define T3_API_NOTICE           "/notice"
#define T3_API_VERSION          "/version"
#define T3_API_CHECK_UPDATE     "/check-update"

#define T3_API_GET_VARIABLE     "/get-variable"
#define T3_API_MODIFY_VARIABLE  "/modify-variable"
#define T3_API_MODIFY_CORE      "/modify-core"
#define T3_API_CLOUD_DOC        "/cloud-doc"

#define T3_API_APP_SIGN         "/app-sign"

/* ==================== 工具函数 ==================== */
static void t3_strncpy_safe(char *dst, const char *src, size_t size)
{
    if (!dst || size == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';
}

static void t3_init_result(t3_result_t *result)
{
    if (!result) return;
    memset(result, 0, sizeof(t3_result_t));
    result->success = 0;
    result->code = 0;
}

/* 获取当前秒级时间戳字符串 */
static char *t3_get_timestamp(void)
{
    static char buf[16];
    time_t now = time(NULL);
    snprintf(buf, sizeof(buf), "%ld", (long)now);
    return buf;
}

/* 对参数值进行加密 + 编码处理 */
static char *t3_encrypt_value(t3_verify_t *handle, const char *value)
{
    if (!value) return strdup("");

    const t3_config_t *cfg = &handle->config;

    /* 未开启请求值加密，直接返回 */
    if (!cfg->request_encrypt || cfg->enc_type == T3_ENC_NONE) {
        return strdup(value);
    }

    unsigned char *encrypted = NULL;
    size_t enc_len = 0;
    size_t value_len = strlen(value);

    switch (cfg->enc_type) {
    case T3_ENC_RC4: {
        encrypted = (unsigned char *)malloc(value_len + 1);
        if (!encrypted) return strdup(value);
        t3_rc4((const unsigned char *)value, value_len,
               cfg->rc4_key, strlen(cfg->rc4_key), encrypted);
        enc_len = value_len;
        break;
    }
    case T3_ENC_DES_ECB: {
        if (cfg->des_key_len < 8) {
            free(encrypted);
            return strdup(value);
        }
        size_t max_len = ((value_len / 8) + 2) * 8;
        encrypted = (unsigned char *)malloc(max_len);
        if (!encrypted) return strdup(value);
        enc_len = t3_des_ecb_encrypt((const unsigned char *)value, value_len,
                                     (const unsigned char *)cfg->des_key, encrypted);
        break;
    }
    case T3_ENC_CUSTOM_BASE64: {
        /* 自定义 Base64 作为"加密"，直接编码 */
        char *out = (char *)malloc(value_len * 2 + 8);
        if (!out) return strdup(value);
        t3_custom_base64_encode((const unsigned char *)value, value_len,
                                cfg->custom_b64_charset, out);
        return out;
    }
    case T3_ENC_RSA:
        /* RSA 实现较复杂，此处预留，建议使用其他加密方式 */
        return strdup(value);
    default:
        return strdup(value);
    }

    if (!encrypted || enc_len == 0) {
        free(encrypted);
        return strdup(value);
    }

    /* 编码 */
    char *encoded = NULL;
    switch (cfg->encode_type) {
    case T3_ENC_BASE64: {
        encoded = (char *)malloc(enc_len * 2 + 8);
        if (encoded)
            t3_base64_encode(encrypted, enc_len, encoded);
        break;
    }
    case T3_ENC_HEX: {
        encoded = (char *)malloc(enc_len * 2 + 2);
        if (encoded)
            t3_hex_encode(encrypted, enc_len, encoded);
        break;
    }
    case T3_ENC_NONE_CODE:
    default: {
        encoded = (char *)malloc(enc_len + 1);
        if (encoded) {
            memcpy(encoded, encrypted, enc_len);
            encoded[enc_len] = '\0';
        }
        break;
    }
    }

    free(encrypted);
    if (!encoded)
        return strdup(value);

    return encoded;
}

/* 对签名值 s 进行加密编码 */
static char *t3_encrypt_sign(t3_verify_t *handle, const char *sign)
{
    return t3_encrypt_value(handle, sign);
}

/* 构建签名原文并计算 MD5 签名
 * params: 已加密编码后的参数数组，格式为 "key=value"
 * param_count: 参数个数
 * 返回: 32 位小写 MD5 字符串（调用者需 free）
 */
static char *t3_calc_signature(t3_verify_t *handle,
                               char **params, int param_count)
{
    const t3_config_t *cfg = &handle->config;

    /* 拼接签名原文: key1=value1&key2=value2&...&APPKEY */
    size_t total_len = strlen(cfg->appkey) + 1;
    for (int i = 0; i < param_count; i++) {
        total_len += strlen(params[i]) + 1;
    }

    char *sign_str = (char *)malloc(total_len + 1);
    if (!sign_str) return NULL;

    sign_str[0] = '\0';
    for (int i = 0; i < param_count; i++) {
        if (i > 0) strcat(sign_str, "&");
        strcat(sign_str, params[i]);
    }
    strcat(sign_str, "&");
    strcat(sign_str, cfg->appkey);

    /* 计算 MD5 */
    char *md5_result = (char *)malloc(MD5_HEX_SIZE);
    if (!md5_result) {
        free(sign_str);
        return NULL;
    }
    md5_hex(sign_str, strlen(sign_str), md5_result);

    free(sign_str);
    return md5_result;
}

/* 解密响应数据 */
static char *t3_decrypt_response(t3_verify_t *handle, const char *response)
{
    if (!response) return NULL;

    const t3_config_t *cfg = &handle->config;

    /* 未开启返回值加密 */
    if (!cfg->response_encrypt || cfg->enc_type == T3_ENC_NONE) {
        return strdup(response);
    }

    size_t resp_len = strlen(response);

    /* 先解码 */
    unsigned char *decoded = (unsigned char *)malloc(resp_len + 1);
    if (!decoded) return strdup(response);

    size_t decoded_len = 0;
    switch (cfg->encode_type) {
    case T3_ENC_HEX:
        decoded_len = t3_hex_decode(response, resp_len, decoded);
        break;
    case T3_ENC_BASE64:
        decoded_len = t3_base64_decode(response, resp_len, decoded);
        break;
    case T3_ENC_NONE_CODE:
    default:
        memcpy(decoded, response, resp_len);
        decoded_len = resp_len;
        break;
    }

    if (decoded_len == 0) {
        free(decoded);
        return strdup(response);
    }

    /* 再解密 */
    char *decrypted = (char *)malloc(decoded_len + 1);
    if (!decrypted) {
        free(decoded);
        return strdup(response);
    }

    switch (cfg->enc_type) {
    case T3_ENC_RC4:
        t3_rc4(decoded, decoded_len, cfg->rc4_key, strlen(cfg->rc4_key),
               (unsigned char *)decrypted);
        decrypted[decoded_len] = '\0';
        break;
    case T3_ENC_DES_ECB: {
        size_t out_len = t3_des_ecb_decrypt(decoded, decoded_len,
                                            (const unsigned char *)cfg->des_key,
                                            (unsigned char *)decrypted);
        decrypted[out_len] = '\0';
        break;
    }
    case T3_ENC_CUSTOM_BASE64: {
        size_t out_len = t3_custom_base64_decode(response, resp_len,
                                                 cfg->custom_b64_charset,
                                                 (unsigned char *)decrypted);
        decrypted[out_len] = '\0';
        break;
    }
    default:
        memcpy(decrypted, decoded, decoded_len);
        decrypted[decoded_len] = '\0';
        break;
    }

    free(decoded);
    return decrypted;
}

/* 构建完整 URL */
static void t3_build_url(t3_verify_t *handle, const char *path,
                         char *url, size_t url_size)
{
    snprintf(url, url_size, "%s%s", handle->config.api_url, path);
}

/* 通用请求执行函数
 * path: 接口路径
 * params: 参数数组，格式为 "key=value"（已加密编码后的值）
 * param_count: 参数个数
 * result: 输出结果
 */
static int t3_do_request(t3_verify_t *handle, const char *path,
                         char **params, int param_count,
                         t3_result_t *result)
{
    t3_init_result(result);

    const t3_config_t *cfg = &handle->config;

    /* 时间戳参数 */
    char *ts_param = NULL;
    if (cfg->timestamp_check) {
        char *ts_enc = t3_encrypt_value(handle, t3_get_timestamp());
        ts_param = (char *)malloc(strlen(ts_enc) + 16);
        sprintf(ts_param, "t=%s", ts_enc);
        free(ts_enc);
    }

    /* 构建签名用的参数数组（不含 s） */
    int sign_param_count = param_count + (ts_param ? 1 : 0);
    char **sign_params = (char **)malloc(sizeof(char *) * sign_param_count);
    int idx = 0;
    for (int i = 0; i < param_count; i++)
        sign_params[idx++] = params[i];
    if (ts_param)
        sign_params[idx++] = ts_param;

    /* 计算签名 */
    char *sign_value = NULL;
    char *sign_param = NULL;
    if (cfg->sign_type != T3_SIGN_OFF) {
        sign_value = t3_calc_signature(handle, sign_params, sign_param_count);
        if (sign_value) {
            char *sign_enc = t3_encrypt_sign(handle, sign_value);
            sign_param = (char *)malloc(strlen(sign_enc) + 16);
            sprintf(sign_param, "s=%s", sign_enc);
            free(sign_enc);
        }
    }

    /* 构建 POST 数据 */
    size_t total_len = 0;
    for (int i = 0; i < param_count; i++)
        total_len += strlen(params[i]) + 1;
    if (ts_param) total_len += strlen(ts_param) + 1;
    if (sign_param) total_len += strlen(sign_param) + 1;

    char *post_data = (char *)malloc(total_len + 1);
    if (!post_data) {
        free(sign_params);
        free(ts_param);
        free(sign_value);
        free(sign_param);
        return -1;
    }
    post_data[0] = '\0';

    for (int i = 0; i < param_count; i++) {
        if (i > 0) strcat(post_data, "&");
        strcat(post_data, params[i]);
    }
    if (ts_param) {
        if (post_data[0]) strcat(post_data, "&");
        strcat(post_data, ts_param);
    }
    if (sign_param) {
        if (post_data[0]) strcat(post_data, "&");
        strcat(post_data, sign_param);
    }

    /* 构建 URL 并发送请求 */
    char url[T3_MAX_URL_LEN];
    t3_build_url(handle, path, url, sizeof(url));

    char *response = NULL;
    size_t resp_len = 0;
    int ret = t3_http_post(url, post_data, &response, &resp_len);

    free(post_data);
    free(sign_params);
    free(ts_param);
    free(sign_value);
    free(sign_param);

    if (ret != 0 || !response) {
        t3_strncpy_safe(result->msg, "HTTP请求失败", sizeof(result->msg));
        return ret;
    }

    /* 保存原始响应 */
    t3_strncpy_safe(result->raw, response, sizeof(result->raw));

    /* 解密响应 */
    char *decrypted = t3_decrypt_response(handle, response);
    free(response);

    if (!decrypted) {
        t3_strncpy_safe(result->msg, "响应解密失败", sizeof(result->msg));
        return -1;
    }

    /* 解析响应 */
    if (cfg->resp_format == T3_RESP_JSON) {
        int code = 0;
        if (t3_json_get_int(decrypted, "code", &code) == 0) {
            result->code = code;
            result->success = (code == 200);
        }
        char msg_buf[1024];
        if (t3_json_get_string(decrypted, "msg", msg_buf, sizeof(msg_buf))) {
            t3_strncpy_safe(result->msg, msg_buf, sizeof(result->msg));
        }
        /* 解析登录相关字段 */
        char buf[512];
        if (t3_json_get_string(decrypted, "statecode", buf, sizeof(buf)))
            t3_strncpy_safe(result->statecode, buf, sizeof(result->statecode));
        if (t3_json_get_string(decrypted, "token", buf, sizeof(buf)))
            t3_strncpy_safe(result->token, buf, sizeof(result->token));
        if (t3_json_get_string(decrypted, "end_time", buf, sizeof(buf)))
            t3_strncpy_safe(result->end_time, buf, sizeof(result->end_time));
        if (t3_json_get_string(decrypted, "date", buf, sizeof(buf)))
            t3_strncpy_safe(result->date, buf, sizeof(result->date));
        int id = 0;
        if (t3_json_get_int(decrypted, "id", &id) == 0)
            result->id = id;
    } else {
        /* 文本格式：成功时返回数据，失败时返回错误描述 */
        t3_strncpy_safe(result->msg, decrypted, sizeof(result->msg));
        /* 文本格式无法区分成功/失败，默认认为成功（内容非空） */
        result->success = (strlen(decrypted) > 0);
        result->code = result->success ? 200 : 201;
    }

    free(decrypted);
    return 0;
}

/* 便捷函数：加密单个参数并构建 "key=value" 字符串 */
static char *t3_make_param(t3_verify_t *handle, const char *key, const char *value)
{
    char *enc = t3_encrypt_value(handle, value);
    char *param = (char *)malloc(strlen(key) + strlen(enc) + 2);
    sprintf(param, "%s=%s", key, enc);
    free(enc);
    return param;
}

/* ==================== 生命周期 ==================== */
t3_verify_t *t3_verify_create(void)
{
    t3_verify_t *handle = (t3_verify_t *)calloc(1, sizeof(t3_verify_t));
    if (!handle) return NULL;
    handle->initialized = 0;
    return handle;
}

void t3_verify_destroy(t3_verify_t *handle)
{
    if (handle) free(handle);
}

int t3_verify_init(t3_verify_t *handle, const t3_config_t *config)
{
    if (!handle || !config) return -1;

    memcpy(&handle->config, config, sizeof(t3_config_t));
    handle->initialized = 1;
    return 0;
}

/* ==================== 工具函数 ==================== */
char *t3_get_machine_code(char *out, size_t out_size)
{
    if (!out || out_size == 0) return NULL;

    /* 尝试获取 MAC 地址作为机器码 */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        /* fallback: 使用 hostname + 时间 */
        snprintf(out, out_size, "UNKNOWN-%ld", (long)getpid());
        return out;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);

    if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
        /* 尝试其他接口 */
        const char *ifaces[] = {"wlan0", "en0", "enp0s3", "ens33", "eth0"};
        for (size_t i = 0; i < sizeof(ifaces)/sizeof(ifaces[0]); i++) {
            strncpy(ifr.ifr_name, ifaces[i], IFNAMSIZ - 1);
            if (ioctl(sock, SIOCGIFHWADDR, &ifr) >= 0)
                break;
        }
    }

    close(sock);

    unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
    if (mac[0] == 0 && mac[1] == 0 && mac[2] == 0) {
        snprintf(out, out_size, "UNKNOWN-%ld", (long)getpid());
    } else {
        snprintf(out, out_size, "%02X%02X%02X%02X%02X%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    return out;
}

/* ==================== 单码卡密接口 ==================== */
int t3_kami_login(t3_verify_t *handle, const char *kami, const char *imei,
                  t3_result_t *result)
{
    if (!handle || !handle->initialized || !kami || !imei) return -1;

    char *params[2];
    params[0] = t3_make_param(handle, "kami", kami);
    params[1] = t3_make_param(handle, "imei", imei);

    int ret = t3_do_request(handle, T3_API_KAMI_LOGIN, params, 2, result);

    free(params[0]);
    free(params[1]);
    return ret;
}

int t3_kami_query(t3_verify_t *handle, const char *kami, t3_result_t *result)
{
    if (!handle || !handle->initialized || !kami) return -1;

    char *params[1];
    params[0] = t3_make_param(handle, "kami", kami);

    int ret = t3_do_request(handle, T3_API_KAMI_QUERY, params, 1, result);

    free(params[0]);
    return ret;
}

int t3_kami_get_core(t3_verify_t *handle, const char *kami, t3_result_t *result)
{
    if (!handle || !handle->initialized || !kami) return -1;

    char *params[1];
    params[0] = t3_make_param(handle, "kami", kami);

    int ret = t3_do_request(handle, T3_API_KAMI_CORE, params, 1, result);

    free(params[0]);
    return ret;
}

int t3_kami_online_count(t3_verify_t *handle, t3_result_t *result)
{
    if (!handle || !handle->initialized) return -1;
    return t3_do_request(handle, T3_API_KAMI_ONLINE, NULL, 0, result);
}

int t3_kami_heartbeat(t3_verify_t *handle, const char *statecode,
                      t3_result_t *result)
{
    if (!handle || !handle->initialized || !statecode) return -1;

    char *params[1];
    params[0] = t3_make_param(handle, "statecode", statecode);

    int ret = t3_do_request(handle, T3_API_KAMI_HEARTBEAT, params, 1, result);

    free(params[0]);
    return ret;
}

/* ==================== 通用接口 ==================== */
int t3_unbind(t3_verify_t *handle, const char *kami_or_user,
              const char *imei, t3_result_t *result)
{
    if (!handle || !handle->initialized || !kami_or_user || !imei) return -1;

    char *params[2];
    params[0] = t3_make_param(handle, "kami", kami_or_user);
    params[1] = t3_make_param(handle, "imei", imei);

    int ret = t3_do_request(handle, T3_API_UNBIND, params, 2, result);

    free(params[0]);
    free(params[1]);
    return ret;
}

int t3_ip_unbind(t3_verify_t *handle, const char *kami_or_user,
                 t3_result_t *result)
{
    if (!handle || !handle->initialized || !kami_or_user) return -1;

    char *params[1];
    params[0] = t3_make_param(handle, "kami", kami_or_user);

    int ret = t3_do_request(handle, T3_API_IP_UNBIND, params, 1, result);

    free(params[0]);
    return ret;
}

int t3_disable(t3_verify_t *handle, const char *kami_or_user,
               t3_result_t *result)
{
    if (!handle || !handle->initialized || !kami_or_user) return -1;

    char *params[1];
    params[0] = t3_make_param(handle, "kami", kami_or_user);

    int ret = t3_do_request(handle, T3_API_DISABLE, params, 1, result);

    free(params[0]);
    return ret;
}

/* ==================== 用户接口 ==================== */
int t3_user_register(t3_verify_t *handle, const char *user,
                     const char *pass, t3_result_t *result)
{
    if (!handle || !handle->initialized || !user || !pass) return -1;

    char *params[2];
    params[0] = t3_make_param(handle, "user", user);
    params[1] = t3_make_param(handle, "pass", pass);

    int ret = t3_do_request(handle, T3_API_USER_REGISTER, params, 2, result);

    free(params[0]);
    free(params[1]);
    return ret;
}

int t3_user_login(t3_verify_t *handle, const char *user,
                  const char *pass, const char *imei, t3_result_t *result)
{
    if (!handle || !handle->initialized || !user || !pass || !imei) return -1;

    char *params[3];
    params[0] = t3_make_param(handle, "user", user);
    params[1] = t3_make_param(handle, "pass", pass);
    params[2] = t3_make_param(handle, "imei", imei);

    int ret = t3_do_request(handle, T3_API_USER_LOGIN, params, 3, result);

    free(params[0]);
    free(params[1]);
    free(params[2]);
    return ret;
}

int t3_user_heartbeat(t3_verify_t *handle, const char *statecode,
                      t3_result_t *result)
{
    if (!handle || !handle->initialized || !statecode) return -1;

    char *params[1];
    params[0] = t3_make_param(handle, "statecode", statecode);

    int ret = t3_do_request(handle, T3_API_USER_HEARTBEAT, params, 1, result);

    free(params[0]);
    return ret;
}

int t3_user_recharge(t3_verify_t *handle, const char *user,
                     const char *card, t3_result_t *result)
{
    if (!handle || !handle->initialized || !user || !card) return -1;

    char *params[2];
    params[0] = t3_make_param(handle, "user", user);
    params[1] = t3_make_param(handle, "card", card);

    int ret = t3_do_request(handle, T3_API_RECHARGE, params, 2, result);

    free(params[0]);
    free(params[1]);
    return ret;
}

int t3_user_change_password(t3_verify_t *handle, const char *user,
                            const char *pass, const char *newpass,
                            t3_result_t *result)
{
    if (!handle || !handle->initialized || !user || !pass || !newpass) return -1;

    char *params[3];
    params[0] = t3_make_param(handle, "user", user);
    params[1] = t3_make_param(handle, "pass", pass);
    params[2] = t3_make_param(handle, "newpass", newpass);

    int ret = t3_do_request(handle, T3_API_CHANGE_PASS, params, 3, result);

    free(params[0]);
    free(params[1]);
    free(params[2]);
    return ret;
}

int t3_user_cancel(t3_verify_t *handle, const char *user,
                   const char *pass, t3_result_t *result)
{
    if (!handle || !handle->initialized || !user || !pass) return -1;

    char *params[2];
    params[0] = t3_make_param(handle, "user", user);
    params[1] = t3_make_param(handle, "pass", pass);

    int ret = t3_do_request(handle, T3_API_USER_CANCEL, params, 2, result);

    free(params[0]);
    free(params[1]);
    return ret;
}

int t3_user_get_core(t3_verify_t *handle, const char *user,
                     const char *pass, t3_result_t *result)
{
    if (!handle || !handle->initialized || !user || !pass) return -1;

    char *params[2];
    params[0] = t3_make_param(handle, "user", user);
    params[1] = t3_make_param(handle, "pass", pass);

    int ret = t3_do_request(handle, T3_API_USER_CORE, params, 2, result);

    free(params[0]);
    free(params[1]);
    return ret;
}

int t3_user_online_count(t3_verify_t *handle, t3_result_t *result)
{
    if (!handle || !handle->initialized) return -1;
    return t3_do_request(handle, T3_API_USER_ONLINE, NULL, 0, result);
}

/* ==================== QQ 授权接口 ==================== */
int t3_bind_qq(t3_verify_t *handle, const char *user, const char *pass,
               const char *openid, const char *access_token,
               t3_result_t *result)
{
    if (!handle || !handle->initialized || !user || !pass || !openid || !access_token)
        return -1;

    char *params[4];
    params[0] = t3_make_param(handle, "user", user);
    params[1] = t3_make_param(handle, "pass", pass);
    params[2] = t3_make_param(handle, "openid", openid);
    params[3] = t3_make_param(handle, "access_token", access_token);

    int ret = t3_do_request(handle, T3_API_BIND_QQ, params, 4, result);

    for (int i = 0; i < 4; i++) free(params[i]);
    return ret;
}

int t3_qq_login(t3_verify_t *handle, const char *openid,
                const char *access_token, t3_result_t *result)
{
    if (!handle || !handle->initialized || !openid || !access_token) return -1;

    char *params[2];
    params[0] = t3_make_param(handle, "openid", openid);
    params[1] = t3_make_param(handle, "access_token", access_token);

    int ret = t3_do_request(handle, T3_API_QQ_LOGIN, params, 2, result);

    free(params[0]);
    free(params[1]);
    return ret;
}

int t3_qq_heartbeat(t3_verify_t *handle, const char *openid,
                    const char *access_token, const char *statecode,
                    t3_result_t *result)
{
    if (!handle || !handle->initialized || !openid || !access_token || !statecode)
        return -1;

    char *params[3];
    params[0] = t3_make_param(handle, "openid", openid);
    params[1] = t3_make_param(handle, "access_token", access_token);
    params[2] = t3_make_param(handle, "statecode", statecode);

    int ret = t3_do_request(handle, T3_API_QQ_HEARTBEAT, params, 3, result);

    for (int i = 0; i < 3; i++) free(params[i]);
    return ret;
}

int t3_qq_get_variable(t3_verify_t *handle, const char *openid,
                       const char *access_token, const char *valueid,
                       const char *valuename, t3_result_t *result)
{
    if (!handle || !handle->initialized || !openid || !access_token ||
        !valueid || !valuename)
        return -1;

    char *params[4];
    params[0] = t3_make_param(handle, "openid", openid);
    params[1] = t3_make_param(handle, "access_token", access_token);
    params[2] = t3_make_param(handle, "valueid", valueid);
    params[3] = t3_make_param(handle, "valuename", valuename);

    int ret = t3_do_request(handle, T3_API_QQ_GET_VAR, params, 4, result);

    for (int i = 0; i < 4; i++) free(params[i]);
    return ret;
}

int t3_qq_get_core(t3_verify_t *handle, const char *openid,
                   const char *access_token, t3_result_t *result)
{
    if (!handle || !handle->initialized || !openid || !access_token) return -1;

    char *params[2];
    params[0] = t3_make_param(handle, "openid", openid);
    params[1] = t3_make_param(handle, "access_token", access_token);

    int ret = t3_do_request(handle, T3_API_QQ_GET_CORE, params, 2, result);

    free(params[0]);
    free(params[1]);
    return ret;
}

int t3_qq_modify_core(t3_verify_t *handle, const char *openid,
                      const char *access_token, const char *core,
                      t3_result_t *result)
{
    if (!handle || !handle->initialized || !openid || !access_token || !core)
        return -1;

    char *params[3];
    params[0] = t3_make_param(handle, "openid", openid);
    params[1] = t3_make_param(handle, "access_token", access_token);
    params[2] = t3_make_param(handle, "core", core);

    int ret = t3_do_request(handle, T3_API_QQ_MODIFY_CORE, params, 3, result);

    for (int i = 0; i < 3; i++) free(params[i]);
    return ret;
}

int t3_qq_unbind(t3_verify_t *handle, const char *user,
                 const char *pass, t3_result_t *result)
{
    if (!handle || !handle->initialized || !user || !pass) return -1;

    char *params[2];
    params[0] = t3_make_param(handle, "user", user);
    params[1] = t3_make_param(handle, "pass", pass);

    int ret = t3_do_request(handle, T3_API_QQ_UNBIND, params, 2, result);

    free(params[0]);
    free(params[1]);
    return ret;
}

/* ==================== 通用工具接口 ==================== */
int t3_heartbeat_any(t3_verify_t *handle, const char *statecode,
                     t3_result_t *result)
{
    if (!handle || !handle->initialized || !statecode) return -1;

    char *params[1];
    params[0] = t3_make_param(handle, "statecode", statecode);

    int ret = t3_do_request(handle, T3_API_HEARTBEAT_ANY, params, 1, result);

    free(params[0]);
    return ret;
}

/* ==================== 程序配置接口 ==================== */
int t3_get_notice(t3_verify_t *handle, t3_result_t *result)
{
    if (!handle || !handle->initialized) return -1;
    return t3_do_request(handle, T3_API_NOTICE, NULL, 0, result);
}

int t3_get_version(t3_verify_t *handle, t3_result_t *result)
{
    if (!handle || !handle->initialized) return -1;
    return t3_do_request(handle, T3_API_VERSION, NULL, 0, result);
}

int t3_check_update(t3_verify_t *handle, const char *ver,
                    t3_result_t *result)
{
    if (!handle || !handle->initialized || !ver) return -1;

    char *params[1];
    params[0] = t3_make_param(handle, "ver", ver);

    int ret = t3_do_request(handle, T3_API_CHECK_UPDATE, params, 1, result);

    free(params[0]);
    return ret;
}

/* ==================== 数据管理接口 ==================== */
int t3_get_variable(t3_verify_t *handle, const char *valueid,
                    const char *valuename, t3_result_t *result)
{
    if (!handle || !handle->initialized || !valueid || !valuename) return -1;

    char *params[2];
    params[0] = t3_make_param(handle, "valueid", valueid);
    params[1] = t3_make_param(handle, "valuename", valuename);

    int ret = t3_do_request(handle, T3_API_GET_VARIABLE, params, 2, result);

    free(params[0]);
    free(params[1]);
    return ret;
}

int t3_modify_variable(t3_verify_t *handle, const char *valueid,
                       const char *valuecontent, t3_result_t *result)
{
    if (!handle || !handle->initialized || !valueid || !valuecontent) return -1;

    char *params[2];
    params[0] = t3_make_param(handle, "valueid", valueid);
    params[1] = t3_make_param(handle, "valuecontent", valuecontent);

    int ret = t3_do_request(handle, T3_API_MODIFY_VARIABLE, params, 2, result);

    free(params[0]);
    free(params[1]);
    return ret;
}

int t3_modify_core(t3_verify_t *handle, const char *core,
                   t3_result_t *result)
{
    if (!handle || !handle->initialized || !core) return -1;

    char *params[1];
    params[0] = t3_make_param(handle, "core", core);

    int ret = t3_do_request(handle, T3_API_MODIFY_CORE, params, 1, result);

    free(params[0]);
    return ret;
}

int t3_get_cloud_doc(t3_verify_t *handle, const char *token,
                     t3_result_t *result)
{
    if (!handle || !handle->initialized || !token) return -1;

    char *params[1];
    params[0] = t3_make_param(handle, "token", token);

    int ret = t3_do_request(handle, T3_API_CLOUD_DOC, params, 1, result);

    free(params[0]);
    return ret;
}

/* ==================== 安全功能接口 ==================== */
int t3_app_sign(t3_verify_t *handle, const char *autograph,
                t3_result_t *result)
{
    if (!handle || !handle->initialized || !autograph) return -1;

    char *params[1];
    params[0] = t3_make_param(handle, "autograph", autograph);

    int ret = t3_do_request(handle, T3_API_APP_SIGN, params, 1, result);

    free(params[0]);
    return ret;
}
