/*
 * T3 SDK 测试程序
 * 演示如何使用 T3 网络验证动态库
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "t3sdk.h"

static void print_result(const char *func_name, const t3_result_t *result)
{
    printf("\n=== %s ===\n", func_name);
    printf("  success: %d\n", result->success);
    printf("  code:    %d\n", result->code);
    printf("  msg:     %s\n", result->msg);
    if (result->statecode[0])
        printf("  statecode: %s\n", result->statecode);
    if (result->token[0])
        printf("  token:   %s\n", result->token);
    if (result->end_time[0])
        printf("  end_time: %s\n", result->end_time);
    printf("  raw:     %.200s\n", result->raw);
}

int main(int argc, char *argv[])
{
    printf("T3 网络验证 SDK 测试程序\n");
    printf("========================\n\n");

    /* 1. 创建 SDK 实例 */
    t3_verify_t *handle = t3_verify_create();
    if (!handle) {
        printf("创建 SDK 实例失败！\n");
        return 1;
    }

    /* 2. 配置 SDK
     *    请根据后台实际配置修改以下参数
     */
    t3_config_t config;
    memset(&config, 0, sizeof(config));

    /* API 地址 - 替换为你的实际 API 域名 + 接口路径前缀 */
    strncpy(config.api_url, "http://your-api-domain.com", sizeof(config.api_url) - 1);

    /* APPKEY - 从后台获取 */
    strncpy(config.appkey, "your_appkey_here", sizeof(config.appkey) - 1);

    /* 加密配置 - 此处示例使用 RC4 + HEX 编码 */
    config.enc_type = T3_ENC_RC4;
    config.encode_type = T3_ENC_HEX;
    config.request_encrypt = 1;   /* 开启请求值加密 */
    config.response_encrypt = 1;  /* 开启返回值加密 */
    strncpy(config.rc4_key, "your_rc4_key", sizeof(config.rc4_key) - 1);

    /* 校验配置 */
    config.timestamp_check = 1;   /* 开启时间戳校验 */
    config.sign_type = T3_SIGN_REQUEST;  /* 请求签名模式 */

    /* 返回值格式 */
    config.resp_format = T3_RESP_JSON;

    /* 初始化 */
    if (t3_verify_init(handle, &config) != 0) {
        printf("SDK 初始化失败！\n");
        t3_verify_destroy(handle);
        return 1;
    }

    printf("SDK 初始化成功\n");

    /* 3. 获取机器码 */
    char imei[64];
    t3_get_machine_code(imei, sizeof(imei));
    printf("机器码: %s\n", imei);

    /* 4. 示例：单码卡密登录 */
    t3_result_t result;
    const char *kami = "TEST_KAMI_123456";  /* 替换为实际卡密 */

    printf("\n--- 测试单码卡密登录 ---\n");
    t3_kami_login(handle, kami, imei, &result);
    print_result("kami_login", &result);

    if (result.success && result.statecode[0]) {
        printf("\n--- 测试心跳验证 ---\n");
        t3_kami_heartbeat(handle, result.statecode, &result);
        print_result("kami_heartbeat", &result);
    }

    /* 5. 示例：获取在线卡密数量 */
    printf("\n--- 测试获取在线卡密数量 ---\n");
    t3_kami_online_count(handle, &result);
    print_result("kami_online_count", &result);

    /* 6. 示例：获取程序公告 */
    printf("\n--- 测试获取程序公告 ---\n");
    t3_get_notice(handle, &result);
    print_result("get_notice", &result);

    /* 7. 示例：获取版本号 */
    printf("\n--- 测试获取版本号 ---\n");
    t3_get_version(handle, &result);
    print_result("get_version", &result);

    /* 8. 示例：用户注册（如需测试用户模式） */
    /*
    printf("\n--- 测试用户注册 ---\n");
    t3_user_register(handle, "testuser", "testpass123", &result);
    print_result("user_register", &result);
    */

    /* 9. 清理 */
    t3_verify_destroy(handle);
    printf("\n测试完成，SDK 已销毁\n");

    return 0;
}
