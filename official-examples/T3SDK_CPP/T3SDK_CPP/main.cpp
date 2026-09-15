/**
 * T3验证SDK - C++版本使用示例
 * 官网: https://www.t3yanzheng.com
 */

#include <iostream>
#include <string>
#include "t3sdk/t3sdk.h"

int main() {
    T3Verify verify;
    
    std::cout << "=== T3验证SDK C++版本使用示例 ===" << std::endl << std::endl;
    
    /* ============================================================
     * 初始化方式一：Base64自定义编码集算法
     * ============================================================ */
    /*
    if (!verify.init(
        "__BASE64_LOGIN_CODE__",
        "__BASE64_NOTICE_CODE__",
        "__BASE64_VERSION_CODE__",
        "__BASE64_HEARTBEAT_CODE__",
        "d633f5e27c1b107cd2a1f98870787263",
        "__BASE64_CHARSET__"
    )) {
        std::cout << "初始化失败！" << std::endl;
        return 1;
    }
    */
    
    /* ============================================================
     * 初始化方式二：RSA 算法 + HEX 编码
     * ============================================================ */
    if (!verify.initRSA(
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
    )) {
        std::cout << "初始化失败！" << std::endl;
        return 1;
    }
    
    std::cout << "SDK初始化成功！" << std::endl << std::endl;
    
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
    std::string machineCode = getMachineCode();
    std::cout << "本机机器码: " << machineCode << std::endl << std::endl;
    
    /* 卡密 */
    std::string card = "Y43495D9E0CEEBAAD095B91999129E96";
    
    /* 本地程序版本号 */
    std::string localVersion = "1000";
    
    /* 1. 获取公告内容 */
    std::cout << "--- 获取公告内容 ---" << std::endl;
    auto noticeResult = verify.getNotice();
    if (noticeResult.success) {
        std::cout << "公告: " << noticeResult.notice << std::endl << std::endl;
    } else {
        std::cout << "获取公告失败: " << noticeResult.error << std::endl << std::endl;
    }
    
    /* 2. 获取最新版本号 */
    std::cout << "--- 获取最新版本号 ---" << std::endl;
    auto versionResult = verify.getLatestVersion();
    if (versionResult.success) {
        std::cout << "最新版本: " << versionResult.version << std::endl;
        if (versionResult.version > localVersion) {
            std::cout << "大于本地版本: " << localVersion << "，请更新本地程序" << std::endl << std::endl;
        } else {
            std::cout << "小于等于本地版本: " << localVersion << "，不需要更新" << std::endl << std::endl;
        }
    } else {
        std::cout << "获取最新版本号失败: " << versionResult.error << std::endl << std::endl;
    }
    
    /* 3. 单码卡密登录 */
    std::cout << "--- 单码卡密登录 ---" << std::endl;
    auto loginResult = verify.login(card, machineCode);
    if (loginResult.success) {
        std::cout << "登录成功！" << std::endl;
        std::cout << "到期时间: " << loginResult.end_time << std::endl;
        std::cout << "卡密时长: " << loginResult.amount << std::endl;
        std::cout << "剩余时间: " << loginResult.available << "秒" << std::endl;
        std::cout << "绑定设备: " << (loginResult.imei.empty() ? "无" : loginResult.imei) << std::endl;
        std::cout << "解绑次数: " << loginResult.change << std::endl;
        std::cout << "核心数据: " << loginResult.core << std::endl << std::endl;
        
        /* 4. 心跳验证 */
        std::cout << "--- 心跳验证 ---" << std::endl;
        auto heartbeatResult = verify.heartbeat(card, loginResult.statecode);
        if (heartbeatResult.success) {
            std::cout << "心跳验证成功！" << std::endl << std::endl;
        } else {
            std::cout << "心跳验证失败: " << heartbeatResult.error << std::endl << std::endl;
        }
    } else {
        std::cout << "登录失败: " << loginResult.error << std::endl << std::endl;
    }
    
    std::cout << "=== 测试完成 ===" << std::endl;
    
    return 0;
}
