/**
 * T3验证SDK - Android NDK 命令行卡密验证示例
 * 官网: https://www.t3yanzheng.com
 *
 * 使用 ndk-build 编译为 Android 可执行文件，通过 adb shell 运行
 *
 * 功能流程:
 * 1. 检查版本号
 * 2. 获取并显示公告
 * 3. 读取本地保存的卡密，尝试自动登录
 * 4. 自动登录失败或无本地卡密，则手动输入卡密
 * 5. 登录成功后打印到期时间，保存卡密到本地文件
 * 6. 登录成功后执行你的代码逻辑
 */

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <atomic>
#include "t3sdk/t3sdk.h"

/* 本地卡密保存文件名（同目录下的隐藏文件） */
static const char* CARD_FILE = ".t3card";

/* 本地程序版本号 */
static const char* LOCAL_VERSION = "1000";

/* ========== 卡密文件读写 ========== */

static std::string loadCard() {
    std::ifstream ifs(CARD_FILE);
    if (!ifs.is_open()) return "";
    std::string card;
    std::getline(ifs, card);
    /* 去除首尾空白 */
    while (!card.empty() && (card.back() == '\n' || card.back() == '\r' || card.back() == ' '))
        card.pop_back();
    return card;
}

static void saveCard(const std::string& card) {
    std::ofstream ofs(CARD_FILE);
    if (ofs.is_open()) {
        ofs << card;
    }
}

/* ========== 主流程 ========== */

int main() {
    T3Verify verify;

    std::cout << "========================================" << std::endl;
    std::cout << "       T3验证 - 命令行验证示例" << std::endl;
    std::cout << "    (Android NDK 可执行文件)" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;

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
        std::cout << "SDK初始化失败！" << std::endl;
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
        std::cout << "SDK初始化失败！" << std::endl;
        return 1;
    }

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

    /* 1. 检查版本号 */
    std::cout << "[版本检查] ";
    auto versionResult = verify.getLatestVersion();
    if (versionResult.success) {
        if (versionResult.version > LOCAL_VERSION) {
            std::cout << "发现新版本: " << versionResult.version
                      << "，当前版本: " << LOCAL_VERSION
                      << "，请更新后再使用" << std::endl;
            return 1;
        }
        std::cout << "当前已是最新版本 (" << LOCAL_VERSION << ")" << std::endl;
    } else {
        std::cout << "获取版本号失败: " << versionResult.error << std::endl;
    }

    /* 2. 获取公告 */
    std::cout << std::endl;
    auto noticeResult = verify.getNotice();
    if (noticeResult.success && !noticeResult.notice.empty()) {
        std::cout << "========== 公告 ==========" << std::endl;
        std::cout << noticeResult.notice << std::endl;
        std::cout << "==========================" << std::endl;
    }
    std::cout << std::endl;

    /* 3. 登录流程 */
    std::string machineCode = getMachineCode();
    std::string card;
    T3LoginResult loginResult;
    bool loggedIn = false;

    /* 尝试从本地文件读取卡密并自动登录 */
    card = loadCard();
    if (!card.empty()) {
        std::cout << "[自动登录] 检测到本地保存的卡密，正在自动登录..." << std::endl;
        loginResult = verify.login(card, machineCode);
        if (loginResult.success) {
            loggedIn = true;
            std::cout << "[自动登录] 登录成功！" << std::endl;
        } else {
            std::cout << "[自动登录] 自动登录失败: " << loginResult.error << std::endl;
            std::cout << "[自动登录] 请手动输入卡密" << std::endl << std::endl;
            card.clear();
        }
    }

    /* 手动输入卡密循环 */
    while (!loggedIn) {
        std::cout << "请输入卡密: ";
        std::getline(std::cin, card);

        /* 去除首尾空白 */
        while (!card.empty() && (card.front() == ' ' || card.front() == '\t'))
            card.erase(card.begin());
        while (!card.empty() && (card.back() == ' ' || card.back() == '\t'
               || card.back() == '\n' || card.back() == '\r'))
            card.pop_back();

        if (card.empty()) {
            std::cout << "卡密不能为空，请重新输入" << std::endl;
            continue;
        }

        std::cout << "[登录中] 正在验证卡密..." << std::endl;
        loginResult = verify.login(card, machineCode);
        if (loginResult.success) {
            loggedIn = true;
            std::cout << "[登录成功]" << std::endl;
        } else {
            std::cout << "[登录失败] " << loginResult.error << std::endl;
            std::cout << "请重新输入卡密" << std::endl << std::endl;
        }
    }

    /* 打印到期时间 */
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  登录成功！" << std::endl;
    std::cout << "  到期时间: " << loginResult.end_time << std::endl;
    std::cout << "  卡密时长: " << loginResult.amount << std::endl;
    std::cout << "  剩余时间: " << loginResult.available << "秒" << std::endl;
    std::cout << "  绑定设备: " << (loginResult.imei.empty() ? "无" : loginResult.imei) << std::endl;
    std::cout << "  解绑次数: " << loginResult.change << std::endl;
    std::cout << "  核心数据: " << loginResult.core << std::endl;
    std::cout << "========================================" << std::endl;

    /* 保存卡密到本地文件 */
    saveCard(card);

    /* ============================================================
     * 心跳验证线程：每 60 秒一次，连续失败 5 次强制退出
     * ============================================================ */

    std::atomic<bool> running(true);
    std::thread heartbeatThread([&]() {
        int failCount = 0;
        const int MAX_FAIL = 5;
        const int INTERVAL = 60;

        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(INTERVAL));
            if (!running) break;

            auto hbResult = verify.heartbeat(card, loginResult.statecode);
            if (hbResult.success) {
                failCount = 0;
                std::cout << "[心跳] 验证成功" << std::endl;
            } else {
                failCount++;
                std::cout << "[心跳] 验证失败 (" << failCount << "/" << MAX_FAIL
                          << "): " << hbResult.error << std::endl;
                if (failCount >= MAX_FAIL) {
                    std::cout << "[心跳] 连续失败 " << MAX_FAIL << " 次，程序强制退出！" << std::endl;
                    exit(1);
                }
            }
        }
    });
    heartbeatThread.detach();

    /* ============================================================
     * 以下是你的代码逻辑（登录成功后执行）
     * ============================================================ */

    std::cout << std::endl;
    std::cout << ">>> 验证通过，开始执行代码逻辑 <<<" << std::endl;
    std::cout << ">>> 心跳验证已启动（每60秒一次）<<<" << std::endl;
    std::cout << std::endl;

    // TODO: 在这里编写你的业务代码
    // 心跳线程在后台持续运行，连续5次失败将强制退出程序
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    running = false;
    return 0;
}
