/**
 * T3 验证 SDK - 测试/示例程序
 *
 * 演示如何使用 T3 网络验证 SDK 的 C 语言接口
 */

#include <stdio.h>
#include <string.h>
#include "t3sdk.h"

int main() {
    T3Verify verify;
    T3LoginResult login_result;
    T3NoticeResult notice_result;
    T3Result result;
    char machine_code[64];

    printf("=== T3 网络验证 SDK 测试程序 ===\n\n");

    /* 1. 获取机器码 */
    if (get_machine_code(machine_code) == 0) {
        printf("[1] 机器码: %s\n", machine_code);
    } else {
        printf("[1] 获取机器码失败\n");
        strcpy(machine_code, "00000000000000000000000000000000");
    }

    /* 2. 初始化 SDK (Base64 模式)
     *
     * 参数说明:
     *   login_code     - 卡密登录调用码 (后台获取)
     *   notice_code    - 获取公告调用码
     *   version_code   - 获取版本号调用码
     *   heartbeat_code - 心跳验证调用码
     *   appkey         - 应用密钥
     *   base64_charset - 自定义 Base64 字符集 (64 个字符)
     */
    const char *login_code     = "你的登录调用码";
    const char *notice_code    = "你的公告调用码";
    const char *version_code   = "你的版本调用码";
    const char *heartbeat_code = "你的心跳调用码";
    const char *appkey         = "你的APPKEY";
    const char *base64_charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    printf("\n[2] 初始化 SDK (Base64 模式)...\n");
    if (t3verify_init(&verify, login_code, notice_code, version_code,
                      heartbeat_code, appkey, base64_charset) == 0) {
        printf("    初始化成功\n");
    } else {
        printf("    初始化失败 (调用码或字符集有误)\n");
        /* 即使失败也继续演示其他功能 */
    }

    /* 3. 设置其他调用码 (可选, 根据后台配置) */
    printf("\n[3] 设置其他调用码...\n");
    t3verify_set_code(&verify, "query",        "查询卡密调用码");
    t3verify_set_code(&verify, "register",     "用户注册调用码");
    t3verify_set_code(&verify, "user_login",   "用户登录调用码");
    t3verify_set_code(&verify, "user_heartbeat","用户心跳调用码");
    t3verify_set_code(&verify, "check_update", "检查更新调用码");
    t3verify_set_code(&verify, "get_variable", "获取变量调用码");
    t3verify_set_code(&verify, "online_kami",  "在线卡密调用码");
    t3verify_set_code(&verify, "online_user",  "在线用户调用码");
    t3verify_set_code(&verify, "cloud_doc",    "云文档调用码");
    t3verify_set_code(&verify, "app_sign",     "应用签名调用码");
    /* ... 其他调用码按需设置 ... */
    printf("    已设置调用码\n");

    /* 4. 卡密登录 (需要有效的调用码和卡密) */
    printf("\n[4] 卡密登录 (kami=测试卡密)...\n");
    if (t3verify_login(&verify, "测试卡密", machine_code, &login_result) == 0) {
        printf("    登录成功!\n");
        printf("    ID: %s\n", login_result.id);
        printf("    到期时间: %s\n", login_result.end_time);
        printf("    状态码: %s\n", login_result.statecode);
        printf("    核心数据: %s\n", login_result.core);

        /* 5. 心跳验证 */
        printf("\n[5] 心跳验证...\n");
        if (t3verify_heartbeat(&verify, "测试卡密", login_result.statecode, &result) == 0) {
            printf("    心跳成功: %s\n", result.msg);
        } else {
            printf("    心跳失败: %s\n", result.error);
        }
    } else {
        printf("    登录失败: %s\n", login_result.error);
        printf("    (提示: 请配置有效的调用码和卡密)\n");
    }

    /* 6. 获取公告 */
    printf("\n[6] 获取公告...\n");
    if (t3verify_get_notice(&verify, &notice_result) == 0) {
        printf("    公告: %s\n", notice_result.notice);
    } else {
        printf("    获取公告失败: %s\n", notice_result.error);
    }

    printf("\n=== 测试完成 ===\n");
    printf("\n使用说明:\n");
    printf("  1. 从 T3 后台获取各接口的调用码\n");
    printf("  2. 获取 APPKEY 和自定义 Base64 字符集\n");
    printf("  3. 替换上面的占位参数\n");
    printf("  4. 重新编译运行\n");
    printf("\nRSA 模式初始化示例:\n");
    printf("  t3verify_init_rsa(&verify, login_code, notice_code,\n");
    printf("      version_code, heartbeat_code, appkey, rsa_public_key);\n");

    return 0;
}
