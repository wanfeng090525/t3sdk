/**
 * T3验证SDK - C语言版本
 * 官网: https://www.t3yanzheng.com
 */

#ifndef T3SDK_H
#define T3SDK_H

#include <stdint.h>

/* 最大字符串长度定义 */
#define T3_SERVER_COUNT 6
#define MAX_URL_LEN 512
#define MAX_CODE_LEN 64
#define MAX_APPKEY_LEN 128
#define MAX_CHARSET_LEN 65
#define MAX_RSA_KEY_LEN 2048
#define MAX_RESPONSE_LEN 8192
#define MAX_ERROR_LEN 256
#define MAX_TOKEN_LEN 64
#define MAX_KAMI_LEN 128
#define MAX_IMEI_LEN 64
#define MAX_STATECODE_LEN 64
#define MAX_END_TIME_LEN 32
#define MAX_NOTICE_LEN 1024
#define MAX_VERSION_LEN 32
#define MAX_MSG_LEN 1024
#define MAX_USER_LEN 128
#define MAX_PASS_LEN 128
#define MAX_QQ_LEN 32
#define MAX_CORE_LEN 256
#define MAX_VALUE_LEN 1024

/* ========== 返回结果结构体 ========== */

typedef struct {
    int success;                      /* 是否成功: 1成功, 0失败 */
    char error[MAX_ERROR_LEN];        /* 错误信息 */
    char msg[MAX_MSG_LEN];            /* 成功提示消息 */
} T3Result;

typedef struct {
    int success;
    char error[MAX_ERROR_LEN];
    char kami[MAX_KAMI_LEN];       /* 本次登录使用的卡密（native 自动登录回填） */
    char id[MAX_TOKEN_LEN];
    char end_time[MAX_END_TIME_LEN];
    char statecode[MAX_STATECODE_LEN];
    char recharge[MAX_TOKEN_LEN];
    char use_time[MAX_TOKEN_LEN];
    char amount[MAX_TOKEN_LEN];
    char available[MAX_TOKEN_LEN];
    char imei[MAX_IMEI_LEN];
    char change[MAX_TOKEN_LEN];
    char core[MAX_CORE_LEN];
} T3LoginResult;

typedef struct {
    int success;
    char error[MAX_ERROR_LEN];
    char notice[MAX_NOTICE_LEN];
} T3NoticeResult;

typedef struct {
    int success;
    char error[MAX_ERROR_LEN];
    char version[MAX_VERSION_LEN];
} T3VersionResult;

typedef struct {
    int success;
    char error[MAX_ERROR_LEN];
    char state[MAX_TOKEN_LEN];
    char use[MAX_TOKEN_LEN];
    char id[MAX_TOKEN_LEN];
    char use_time[MAX_TOKEN_LEN];
    char end_time[MAX_END_TIME_LEN];
    char line_time[MAX_TOKEN_LEN];
    char line[MAX_TOKEN_LEN];
    char amount[MAX_TOKEN_LEN];
    char available[MAX_TOKEN_LEN];
} T3QueryResult;

typedef struct {
    int success;
    char error[MAX_ERROR_LEN];
    int has_update;                   /* 1:有更新, 0:无更新 */
    char ver[MAX_VERSION_LEN];
    char version[MAX_VERSION_LEN];
    char uplog[MAX_MSG_LEN];
    char upurl[MAX_URL_LEN];
    char msg[MAX_MSG_LEN];
} T3UpdateResult;

typedef struct {
    int success;
    char error[MAX_ERROR_LEN];
    char value[MAX_VALUE_LEN];
} T3VariableResult;

typedef struct {
    int success;
    char error[MAX_ERROR_LEN];
    char content[MAX_MSG_LEN];
} T3CloudDocResult;

typedef struct {
    int success;
    char error[MAX_ERROR_LEN];
    char core[MAX_CORE_LEN];
} T3CoreResult;

typedef struct {
    int success;
    char error[MAX_ERROR_LEN];
    int count;
} T3OnlineResult;

typedef struct {
    int success;
    char error[MAX_ERROR_LEN];
    char msg[MAX_MSG_LEN];
    char autograph[MAX_MSG_LEN];
    long time;
} T3AppSignResult;

/* ========== T3验证SDK主结构体 ========== */

typedef struct {
    char server_urls[T3_SERVER_COUNT][MAX_URL_LEN];
    int server_count;
    char server_url[MAX_URL_LEN];
    /* 已有调用码 */
    char login_code[MAX_CODE_LEN];
    char notice_code[MAX_CODE_LEN];
    char version_code[MAX_CODE_LEN];
    char heartbeat_code[MAX_CODE_LEN];
    /* 新增调用码 */
    char query_code[MAX_CODE_LEN];
    char register_code[MAX_CODE_LEN];
    char user_login_code[MAX_CODE_LEN];
    char user_heartbeat_code[MAX_CODE_LEN];
    char qq_login_code[MAX_CODE_LEN];
    char bind_qq_code[MAX_CODE_LEN];
    char change_password_code[MAX_CODE_LEN];
    char user_cancel_code[MAX_CODE_LEN];
    char recharge_code[MAX_CODE_LEN];
    char kami_recharge_code[MAX_CODE_LEN];
    char unbind_code[MAX_CODE_LEN];
    char ip_unbind_code[MAX_CODE_LEN];
    char disable_code[MAX_CODE_LEN];
    char check_update_code[MAX_CODE_LEN];
    char get_variable_code[MAX_CODE_LEN];
    char modify_variable_code[MAX_CODE_LEN];
    char modify_core_code[MAX_CODE_LEN];
    char get_kami_core_code[MAX_CODE_LEN];
    char get_user_core_code[MAX_CODE_LEN];
    char online_kami_code[MAX_CODE_LEN];
    char online_user_code[MAX_CODE_LEN];
    char cloud_doc_code[MAX_CODE_LEN];
    char app_sign_code[MAX_CODE_LEN];
    char qq_heartbeat_code[MAX_CODE_LEN];
    char get_variable_by_qq_code[MAX_CODE_LEN];
    char get_user_core_by_qq_code[MAX_CODE_LEN];
    char modify_user_core_by_qq_code[MAX_CODE_LEN];
    char unbind_qq_code[MAX_CODE_LEN];
    char heartbeat_any_code[MAX_CODE_LEN];
    /* 公共配置 */
    char appkey[MAX_APPKEY_LEN];
    char base64_charset[MAX_CHARSET_LEN];
    char rsa_public_key[MAX_RSA_KEY_LEN];
    int encode_type;                  /* 编码类型: 0=base64, 1=rsa */
    char statecode[MAX_STATECODE_LEN];
    char end_time[MAX_END_TIME_LEN];
    int initialized;
} T3Verify;

/* ========== 公共函数 ========== */

int get_machine_code(char *machine_code);
/* 计算字符串的MD5哈希(16进制大写)，输出33字节缓冲区 */
void md5_string_upper(const char *str, char output[33]);

/* ========== 初始化 ========== */

int t3verify_init(T3Verify *verify, const char *login_code, const char *notice_code,
    const char *version_code, const char *heartbeat_code,
    const char *appkey, const char *base64_charset);

int t3verify_init_rsa(T3Verify *verify, const char *login_code, const char *notice_code,
    const char *version_code, const char *heartbeat_code,
    const char *appkey, const char *rsa_public_key);

/* 设置新增调用码 */
void t3verify_set_code(T3Verify *verify, const char *field, const char *code);

/* ========== 卡密验证 ========== */

int t3verify_login(T3Verify *verify, const char *kami, const char *imei, T3LoginResult *result);
int t3verify_query_kami(T3Verify *verify, const char *kami, T3QueryResult *result);
int t3verify_heartbeat(T3Verify *verify, const char *kami, const char *statecode, T3Result *result);

/* ========== 数据与内容 ========== */

int t3verify_get_notice(T3Verify *verify, T3NoticeResult *result);
int t3verify_get_latest_version(T3Verify *verify, T3VersionResult *result);
int t3verify_check_update(T3Verify *verify, const char *ver, T3UpdateResult *result);
int t3verify_get_cloud_doc(T3Verify *verify, const char *token, T3CloudDocResult *result);
int t3verify_app_sign(T3Verify *verify, const char *autograph, T3AppSignResult *result);

/* ========== 用户体系 ========== */

int t3verify_user_register(T3Verify *verify, const char *user, const char *pass, const char *email, T3Result *result);
int t3verify_user_login(T3Verify *verify, const char *user, const char *pass, const char *imei, T3LoginResult *result);
int t3verify_user_heartbeat(T3Verify *verify, const char *user, const char *pass, const char *statecode, T3Result *result);
int t3verify_qq_login(T3Verify *verify, const char *openid, const char *access_token, T3LoginResult *result);
int t3verify_bind_qq(T3Verify *verify, const char *user, const char *pass, const char *openid, const char *access_token, T3Result *result);
int t3verify_qq_heartbeat(T3Verify *verify, const char *openid, const char *access_token, const char *statecode, T3Result *result);
int t3verify_get_variable_by_qq(T3Verify *verify, const char *openid, const char *access_token, const char *valueid, const char *valuename, T3VariableResult *result);
int t3verify_get_user_core_by_qq(T3Verify *verify, const char *openid, const char *access_token, T3CoreResult *result);
int t3verify_modify_user_core_by_qq(T3Verify *verify, const char *openid, const char *access_token, const char *core, T3Result *result);
int t3verify_unbind_qq(T3Verify *verify, const char *user, const char *pass, T3Result *result);
int t3verify_heartbeat_any(T3Verify *verify, const char *statecode, T3Result *result);
int t3verify_change_password(T3Verify *verify, const char *user, const char *oldpass, const char *newpass, T3Result *result);
int t3verify_user_cancel(T3Verify *verify, const char *user, const char *pass, T3Result *result);
int t3verify_recharge(T3Verify *verify, const char *user, const char *card, T3Result *result);
int t3verify_kami_recharge(T3Verify *verify, const char *target_kami, const char *source_kami, T3Result *result);

/* ========== 设备与安全 ========== */

int t3verify_unbind_kami(T3Verify *verify, const char *kami, const char *imei, T3Result *result);
int t3verify_unbind_user(T3Verify *verify, const char *user, const char *pass, const char *imei, T3Result *result);
int t3verify_ip_unbind_kami(T3Verify *verify, const char *kami, T3Result *result);
int t3verify_ip_unbind_user(T3Verify *verify, const char *user, const char *pass, T3Result *result);
int t3verify_disable_kami(T3Verify *verify, const char *kami, T3Result *result);
int t3verify_disable_user(T3Verify *verify, const char *user, const char *pass, T3Result *result);

/* ========== 远程变量 ========== */

int t3verify_get_variable_by_kami(T3Verify *verify, const char *kami, const char *valueid, const char *valuename, T3VariableResult *result);
int t3verify_get_variable_by_user(T3Verify *verify, const char *user, const char *pass, const char *valueid, const char *valuename, T3VariableResult *result);
int t3verify_modify_variable_by_kami(T3Verify *verify, const char *kami, const char *valueid, const char *valuecontent, T3Result *result);
int t3verify_modify_variable_by_user(T3Verify *verify, const char *user, const char *pass, const char *valueid, const char *valuecontent, T3Result *result);

/* ========== 核心数据 ========== */

int t3verify_modify_core_by_kami(T3Verify *verify, const char *kami, const char *core, T3Result *result);
int t3verify_modify_core_by_user(T3Verify *verify, const char *user, const char *pass, const char *core, T3Result *result);
int t3verify_get_core_by_kami(T3Verify *verify, const char *kami, T3CoreResult *result);
int t3verify_get_core_by_user(T3Verify *verify, const char *user, const char *pass, T3CoreResult *result);

/* ========== 在线数量 ========== */

int t3verify_get_online_kami_count(T3Verify *verify, T3OnlineResult *result);
int t3verify_get_online_user_count(T3Verify *verify, T3OnlineResult *result);

#endif /* T3SDK_H */
