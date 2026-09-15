/*
 * T3 网络验证 SDK - C 语言动态库接口
 * 版本: 1.0.0
 *
 * 支持全部 33 个标准 WebAPI 接口
 * 支持 RC4 / DES-ECB / 自定义Base64 / RSA 加密
 * 支持 Base64 / HEX 编码
 * 支持请求签名 / 双向签名校验
 */
#ifndef T3_SDK_H
#define T3_SDK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */
#define T3_MAX_URL_LEN     512
#define T3_MAX_APPKEY_LEN  128
#define T3_MAX_KEY_LEN     256
#define T3_MAX_RESP_LEN    65536
#define T3_MAX_STATE_LEN   256
#define T3_MAX_TOKEN_LEN   128

/* ==================== 枚举类型 ==================== */
/* 加密算法类型 */
typedef enum {
    T3_ENC_NONE = 0,       /* 不加密 */
    T3_ENC_RC4,            /* RC4 对称流式加密 */
    T3_ENC_DES_ECB,        /* DES-ECB 对称分组加密 */
    T3_ENC_CUSTOM_BASE64,  /* 自定义 Base64 编码 */
    T3_ENC_RSA             /* RSA 非对称加密 */
} t3_enc_type_t;

/* 编码方式 */
typedef enum {
    T3_ENC_NONE_CODE = 0,  /* 无编码 */
    T3_ENC_BASE64,         /* Base64 编码 */
    T3_ENC_HEX             /* HEX 编码（推荐） */
} t3_encode_type_t;

/* 签名模式 */
typedef enum {
    T3_SIGN_OFF = 0,       /* 关闭签名 */
    T3_SIGN_REQUEST,       /* 请求签名 */
    T3_SIGN_BOTH           /* 双向签名 */
} t3_sign_type_t;

/* 返回值格式 */
typedef enum {
    T3_RESP_TEXT = 0,      /* 文本格式 */
    T3_RESP_JSON           /* JSON 格式 */
} t3_resp_format_t;

/* ==================== 响应结果结构 ==================== */
typedef struct {
    int    success;        /* 1 成功，0 失败 */
    int    code;           /* 200 成功，201 失败 */
    char   msg[1024];      /* 成功时为数据，失败时为错误描述 */
    /* 登录相关字段 */
    char   statecode[T3_MAX_STATE_LEN];  /* 登录状态码，用于心跳 */
    char   token[T3_MAX_TOKEN_LEN];      /* Token（双向签名时返回） */
    char   end_time[64];   /* 到期时间 */
    long   id;             /* 用户/卡密 ID */
    char   date[16];       /* 日期分钟 */
    /* 通用原始响应 */
    char   raw[T3_MAX_RESP_LEN];
} t3_result_t;

/* ==================== 配置结构 ==================== */
typedef struct {
    /* API 基础配置 */
    char     api_url[T3_MAX_URL_LEN];      /* API 调用地址（不含接口路径） */

    /* 安全配置 */
    char     appkey[T3_MAX_APPKEY_LEN];    /* APPKEY */
    t3_enc_type_t  enc_type;               /* 加密算法 */
    t3_encode_type_t encode_type;          /* 请求值编码方式 */
    int      request_encrypt;              /* 是否开启请求值加密 */
    int      response_encrypt;             /* 是否开启返回值加密 */

    /* 密钥 */
    char     rc4_key[T3_MAX_KEY_LEN];      /* RC4 密钥 */
    char     des_key[16];                  /* DES 密钥（8字节，可能含'\0'） */
    int      des_key_len;                  /* DES 密钥长度 */
    char     custom_b64_charset[128];      /* 自定义 Base64 编码集（64字符） */
    char     rsa_public_key[4096];         /* RSA 公钥 PEM */

    /* 校验配置 */
    int      timestamp_check;              /* 是否开启时间戳校验 */
    t3_sign_type_t sign_type;              /* 签名模式 */

    /* 返回值配置 */
    t3_resp_format_t resp_format;          /* 返回值格式 */
} t3_config_t;

/* ==================== SDK 句柄 ==================== */
typedef struct t3_verify t3_verify_t;

/* ==================== 生命周期 ==================== */
/* 创建 SDK 实例 */
t3_verify_t *t3_verify_create(void);

/* 销毁 SDK 实例 */
void t3_verify_destroy(t3_verify_t *handle);

/* 初始化配置
 * 返回: 0 成功，非 0 失败
 */
int t3_verify_init(t3_verify_t *handle, const t3_config_t *config);

/* ==================== 工具函数 ==================== */
/* 获取设备机器码（基于 MAC 地址等硬件信息） */
char *t3_get_machine_code(char *out, size_t out_size);

/* ==================== 单码卡密接口 (5) ==================== */
/* 单码卡密登录 */
int t3_kami_login(t3_verify_t *handle, const char *kami, const char *imei,
                  t3_result_t *result);

/* 查询单码卡密状态（不触发登录） */
int t3_kami_query(t3_verify_t *handle, const char *kami, t3_result_t *result);

/* 获取卡密核心数据 */
int t3_kami_get_core(t3_verify_t *handle, const char *kami, t3_result_t *result);

/* 获取在线卡密数量 */
int t3_kami_online_count(t3_verify_t *handle, t3_result_t *result);

/* 单码心跳验证 */
int t3_kami_heartbeat(t3_verify_t *handle, const char *statecode,
                      t3_result_t *result);

/* ==================== 通用接口 (3) ==================== */
/* 解绑机器码 */
int t3_unbind(t3_verify_t *handle, const char *kami_or_user,
              const char *imei, t3_result_t *result);

/* IP 解绑 */
int t3_ip_unbind(t3_verify_t *handle, const char *kami_or_user,
                 t3_result_t *result);

/* 禁用卡密/用户 */
int t3_disable(t3_verify_t *handle, const char *kami_or_user,
               t3_result_t *result);

/* ==================== 用户接口 (8) ==================== */
/* 用户注册 */
int t3_user_register(t3_verify_t *handle, const char *user,
                     const char *pass, t3_result_t *result);

/* 用户登录 */
int t3_user_login(t3_verify_t *handle, const char *user,
                  const char *pass, const char *imei, t3_result_t *result);

/* 用户心跳验证 */
int t3_user_heartbeat(t3_verify_t *handle, const char *statecode,
                      t3_result_t *result);

/* 用户充值卡密 */
int t3_user_recharge(t3_verify_t *handle, const char *user,
                     const char *card, t3_result_t *result);

/* 用户修改密码 */
int t3_user_change_password(t3_verify_t *handle, const char *user,
                            const char *pass, const char *newpass,
                            t3_result_t *result);

/* 用户注销账号 */
int t3_user_cancel(t3_verify_t *handle, const char *user,
                   const char *pass, t3_result_t *result);

/* 获取用户核心数据 */
int t3_user_get_core(t3_verify_t *handle, const char *user,
                     const char *pass, t3_result_t *result);

/* 获取在线用户数量 */
int t3_user_online_count(t3_verify_t *handle, t3_result_t *result);

/* ==================== QQ 授权接口 (7) ==================== */
/* 用户绑定 QQ */
int t3_bind_qq(t3_verify_t *handle, const char *user, const char *pass,
               const char *openid, const char *access_token,
               t3_result_t *result);

/* QQ 登录 */
int t3_qq_login(t3_verify_t *handle, const char *openid,
                const char *access_token, t3_result_t *result);

/* QQ 心跳验证 */
int t3_qq_heartbeat(t3_verify_t *handle, const char *openid,
                    const char *access_token, const char *statecode,
                    t3_result_t *result);

/* QQ 获取远程变量 */
int t3_qq_get_variable(t3_verify_t *handle, const char *openid,
                       const char *access_token, const char *valueid,
                       const char *valuename, t3_result_t *result);

/* QQ 获取核心数据 */
int t3_qq_get_core(t3_verify_t *handle, const char *openid,
                   const char *access_token, t3_result_t *result);

/* QQ 修改核心数据 */
int t3_qq_modify_core(t3_verify_t *handle, const char *openid,
                      const char *access_token, const char *core,
                      t3_result_t *result);

/* 用户解绑 QQ */
int t3_qq_unbind(t3_verify_t *handle, const char *user,
                 const char *pass, t3_result_t *result);

/* ==================== 通用工具接口 (1) ==================== */
/* 统一心跳验证（自动识别单码/用户/QQ） */
int t3_heartbeat_any(t3_verify_t *handle, const char *statecode,
                     t3_result_t *result);

/* ==================== 程序配置接口 (3) ==================== */
/* 获取程序公告 */
int t3_get_notice(t3_verify_t *handle, t3_result_t *result);

/* 获取版本号 */
int t3_get_version(t3_verify_t *handle, t3_result_t *result);

/* 检查更新 */
int t3_check_update(t3_verify_t *handle, const char *ver,
                    t3_result_t *result);

/* ==================== 数据管理接口 (4) ==================== */
/* 获取远程变量 */
int t3_get_variable(t3_verify_t *handle, const char *valueid,
                    const char *valuename, t3_result_t *result);

/* 修改远程变量 */
int t3_modify_variable(t3_verify_t *handle, const char *valueid,
                       const char *valuecontent, t3_result_t *result);

/* 修改核心数据 */
int t3_modify_core(t3_verify_t *handle, const char *core,
                   t3_result_t *result);

/* 获取云文档 */
int t3_get_cloud_doc(t3_verify_t *handle, const char *token,
                     t3_result_t *result);

/* ==================== 安全功能接口 (1) ==================== */
/* 判断应用签名 */
int t3_app_sign(t3_verify_t *handle, const char *autograph,
                t3_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* T3_SDK_H */
