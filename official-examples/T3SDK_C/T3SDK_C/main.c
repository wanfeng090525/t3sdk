/**
 * T3验证SDK - C语言版本使用示例
 * 官网: https://www.t3yanzheng.com
 */

#include <stdio.h>
#include <string.h>
#include "t3sdk/t3sdk.h"

int main() {
    T3Verify verify;
    T3LoginResult login_result;
    T3NoticeResult notice_result;
    T3VersionResult version_result;
    T3Result heartbeat_result;
    char machine_code[64];
    
    printf("=== T3验证SDK C语言版本使用示例 ===\n\n");
    
    /* ============================================================
     * 初始化方式一：Base64自定义编码集算法
     * ============================================================ */
    /*
    if (t3verify_init(
        &verify,
        "__BASE64_LOGIN_CODE__",
        "__BASE64_NOTICE_CODE__",
        "__BASE64_VERSION_CODE__",
        "__BASE64_HEARTBEAT_CODE__",
        "d633f5e27c1b107cd2a1f98870787263",
        "__BASE64_CHARSET__"
    ) < 0) {
        printf("初始化失败！\n");
        return 1;
    }
    */

    /* ============================================================
     * 初始化方式二：RSA 算法 + HEX 编码
     * ============================================================ */
    if (t3verify_init_rsa(
        &verify,
        "813B2676E9690C89",           /* 单码登录调用码 */
        "EC56923E2FD91C99",           /* 获取程序公告调用码 */
        "EA44543183C3F5D3",           /* 获取程序最新版本号调用码 */
        "9AB469F061FA45F4",           /* 单码卡密心跳验证调用码 */
        "d633f5e27c1b107cd2a1f98870787263", /* 程序密钥APPKEY */
        "-----BEGIN PUBLIC KEY-----\n"
        "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDAQP0fmaGhF/sEskSVfDALBG2X\n"
        "KFCtn2HjJj0W+LQOL4bQIyg7Dh1lVUnTSodUwehXGloXHthU/c/Aio7xnYJILewg\n"
        "5QVYKjGbbexgO61KIg0AotYxV8KNUOAg8qPVfsQ+hELwJHAOFHfORSn/fZfd2hVg\n"
        "+YfzVfYS6KW/i0imOQIDAQAB\n"
        "-----END PUBLIC KEY-----"    /* RSA公钥 */
    ) < 0) {
        printf("初始化失败！\n");
        return 1;
    }
    
    printf("SDK初始化成功！\n\n");
    
    /* 使用 RSA 算法时，请确保后台做如下配置:
     * 1. 传输配置-加密配置-全局加解密-开启
     * 2. 传输配置-加密配置-加密算法-RSA算法
     * 3. 传输配置-加密配置-请求值加密-开启
     * 4. 传输配置-加密配置-请求值编码-HEX编码(16进制)
     * 5. 传输配置-加密配置-返回值加密-开启
     * 6. 传输配置-校验配置-时间戳校验-开启
     * 7. 传输配置-校验配置-签名校验-双向签名
     * 8. 传输配置-校验配置-返回值配置-返回值格式-JSON
     * 9. 传输配置-校验配置-返回值配置-JSON返回时间戳-开启
     * 10. 传输配置-校验配置-返回值配置-JSON_CODE类型-int
     */
    
    /* 获取本机机器码 */
    if (get_machine_code(machine_code) < 0) {
        printf("获取机器码失败！\n");
        return 1;
    }
    printf("本机机器码: %s\n\n", machine_code);
    
    /* 卡密 */
    const char *card = "Y43495D9E0CEEBAAD095B91999129E96";
    
    /* 本地程序版本号 */
    const char *local_version = "1000";
    
    /* 1. 获取公告内容 */
    printf("--- 获取公告内容 ---\n");
    if (t3verify_get_notice(&verify, &notice_result) == 0 && notice_result.success) {
        printf("公告: %s\n\n", notice_result.notice);
    } else {
        printf("获取公告失败: %s\n\n", notice_result.error);
    }
    
    /* 2. 获取最新版本号 */
    printf("--- 获取最新版本号 ---\n");
    if (t3verify_get_latest_version(&verify, &version_result) == 0 && version_result.success) {
        printf("最新版本: %s\n", version_result.version);
        
        /* 版本比较 */
        if (strcmp(version_result.version, local_version) > 0) {
            printf("大于本地版本: %s，请更新本地程序\n\n", local_version);
        } else {
            printf("小于等于本地版本: %s，不需要更新\n\n", local_version);
        }
    } else {
        printf("获取最新版本号失败: %s\n\n", version_result.error);
    }
    
    /* 3. 单码卡密登录 */
    printf("--- 单码卡密登录 ---\n");
    if (t3verify_login(&verify, card, machine_code, &login_result) == 0 && login_result.success) {
        printf("登录成功！\n");
        printf("到期时间: %s\n", login_result.end_time);
        printf("卡密时长: %s\n", login_result.amount);
        printf("剩余时间: %s秒\n", login_result.available);
        printf("绑定设备: %s\n", strlen(login_result.imei) > 0 ? login_result.imei : "无");
        printf("解绑次数: %s\n", login_result.change);
        printf("核心数据: %s\n\n", login_result.core);
        
        /* 4. 心跳验证 */
        printf("--- 心跳验证 ---\n");
        if (t3verify_heartbeat(&verify, card, login_result.statecode, &heartbeat_result) == 0 
            && heartbeat_result.success) {
            printf("心跳验证成功！\n\n");
        } else {
            printf("心跳验证失败: %s\n\n", heartbeat_result.error);
        }
    } else {
        printf("登录失败: %s\n\n", login_result.error);
    }
    
    printf("=== 测试完成 ===\n");
    
    return 0;
}
