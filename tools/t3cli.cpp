/**
 * t3cli - T3 网络验证命令行工具（对接 libt3sdk.so 同一套 C++ 接口）
 *
 * 编译:
 *   g++ -std=c++17 -O2 -o t3cli t3cli.cpp ../versions/official-cpp/android/jni/t3sdk/t3sdk.cpp \
 *       -I ../versions/official-cpp/android/jni/t3sdk
 *
 * 用法:
 *   ./t3cli machine
 *   ./t3cli login <卡密>
 *   ./t3cli heartbeat <卡密> <statecode>
 *   ./t3cli notice
 *   ./t3cli version
 *   ./t3cli update <当前版本号>
 *   ./t3cli query <卡密>
 *   ./t3cli unbind_kami <卡密> <机器码>
 *   ...（见底部 usage）
 *
 * 输出: 统一为 JSON（stdout），错误原因在 error 字段。
 * 内置凭证与 libt3sdk.so 完全一致，不要改动。
 */

#include "t3sdk.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <map>
#include <vector>

/* ============================================================
 * T3 后台凭证 - 与 libt3sdk.so 内置完全一致，不要改动
 * ============================================================ */
#define T3_LOGIN_CODE     "813B2676E9690C89"
#define T3_NOTICE_CODE    "EC56923E2FD91C99"
#define T3_VERSION_CODE   "EA44543183C3F5D3"
#define T3_HEARTBEAT_CODE "9AB469F061FA45F4"
#define T3_APPKEY         "d633f5e27c1b107cd2a1f98870787263"
#define T3_RSA_PUBLIC_KEY "-----BEGIN PUBLIC KEY-----\n" \
                          "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDAQP0fmaGhF/sEskSVfDALBG2X\n" \
                          "KFCtn2HjJj0W+LQOL4bQIyg7Dh1lVUnTSodUwehXGloXHthU/c/Aio7xnYJILewg\n" \
                          "5QVYKjGbbexgO61KIg0AotYxV8KNUOAg8qPVfsQ+hELwJHAOFHfORSn/fZfd2hVg\n" \
                          "+YfzVfYS6KW/i0imOQIDAQAB\n" \
                          "-----END PUBLIC KEY-----"

/* 其余功能调用码（与 .so 内置一致） */
struct CodeEntry { const char* field; const char* code; };
static const CodeEntry EXTRA_CODES[] = {
    {"query",        "A2AC50154CC51A76"},
    {"register",     "6B9EABFF00B80750"},
    {"userlogin",    "ECCFEFE957984480"},
    {"userheart",    "EF796F4016C97A66"},
    {"qqlogin",      "B6652EB5C78F0641"},
    {"bindqq",       "BF0378334FF0F0F5"},
    {"changepwd",    "B7C4CD0FAE40AE8C"},
    {"usercancel",   "A71666D3D237E922"},
    {"recharge",     "57559AB00DE6A9DE"},
    {"kamirecharge", "A4634B5AC1F5341A"},
    {"unbind",       "2CA5F5986D945633"},
    {"ipunbind",     "C21B2219566669B9"},
    {"disable",      "CF41580160F0AE75"},
    {"checkupdate",  "890829CBEFA61A56"},
    {"getvariable",  "A015ABD9403AC64A"},
    {"modifyvar",    "78CFB294B32DC2A3"},
    {"modifycore",   "F9CE457547B9DD0F"},
    {"getcore",      "8C58C52B3053820A"},
    {"getusercore",  "CD79FE55F3CE48B2"},
    {"onlinekami",   "B61D29EBFA5E3CF5"},
    {"onlineuser",   "0D46101793B42DD3"},
    {"clouddoc",     "F50B362FC86CC38E"},
    {"appsign",      "BAB98433A890FFB0"},
    {"qqheart",      "77C9731065BAB2A4"},
    {"getvarbyqq",   "2CE5BD9B12733032"},
    {"getcorebyqq",  "5EF62F8A7630DC9A"},
    {"modifycorebyqq", "ACA861E2C03E9385"},
    {"unbindqq",     "8E113021CB8FE80E"},
    {"heartany",     "A0D682FB88F82D31"},
};

/* ========== JSON 输出辅助 ========== */

static std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if ((unsigned char)c < 0x20) { /* 丢弃控制字符 */ }
        else out += c;
    }
    return out;
}

static void print_bool(const char* key, bool v) { printf(",\"%s\":%s", key, v ? "true" : "false"); }
static void print_str(const char* key, const std::string& v) { printf(",\"%s\":\"%s\"", key, json_escape(v).c_str()); }
static void print_int(const char* key, long v) { printf(",\"%s\":%ld", key, v); }

static void print_result(const T3Result& r) {
    printf("{\"success\":%s", r.success ? "true" : "false");
    print_str("error", r.error);
    print_str("msg", r.msg);
    printf("}\n");
}

static void print_login(const T3LoginResult& r) {
    printf("{\"success\":%s", r.success ? "true" : "false");
    print_str("error", r.error);
    print_str("id", r.id);
    print_str("endTime", r.end_time);
    print_str("statecode", r.statecode);
    print_str("recharge", r.recharge);
    print_str("useTime", r.use_time);
    print_str("amount", r.amount);
    print_str("available", r.available);
    print_str("imei", r.imei);
    print_str("change", r.change);
    print_str("core", r.core);
    printf("}\n");
}

static void print_query(const T3QueryResult& r) {
    printf("{\"success\":%s", r.success ? "true" : "false");
    print_str("error", r.error);
    print_str("state", r.state);
    print_str("use", r.use);
    print_str("id", r.id);
    print_str("useTime", r.use_time);
    print_str("endTime", r.end_time);
    print_str("lineTime", r.line_time);
    print_str("line", r.line);
    print_str("amount", r.amount);
    print_str("available", r.available);
    printf("}\n");
}

static void print_notice(const T3NoticeResult& r) {
    printf("{\"success\":%s", r.success ? "true" : "false");
    print_str("error", r.error);
    print_str("notice", r.notice);
    printf("}\n");
}

static void print_version(const T3VersionResult& r) {
    printf("{\"success\":%s", r.success ? "true" : "false");
    print_str("error", r.error);
    print_str("version", r.version);
    printf("}\n");
}

static void print_update(const T3UpdateResult& r) {
    printf("{\"success\":%s", r.success ? "true" : "false");
    print_str("error", r.error);
    print_bool("hasUpdate", r.hasUpdate);
    print_str("ver", r.ver);
    print_str("version", r.version);
    print_str("uplog", r.uplog);
    print_str("upurl", r.upurl);
    print_str("msg", r.msg);
    printf("}\n");
}

static void print_variable(const T3VariableResult& r) {
    printf("{\"success\":%s", r.success ? "true" : "false");
    print_str("error", r.error);
    print_str("value", r.value);
    printf("}\n");
}

static void print_core(const T3CoreResult& r) {
    printf("{\"success\":%s", r.success ? "true" : "false");
    print_str("error", r.error);
    print_str("core", r.core);
    printf("}\n");
}

static void print_clouddoc(const T3CloudDocResult& r) {
    printf("{\"success\":%s", r.success ? "true" : "false");
    print_str("error", r.error);
    print_str("content", r.content);
    printf("}\n");
}

static void print_online(const T3OnlineResult& r) {
    printf("{\"success\":%s", r.success ? "true" : "false");
    print_str("error", r.error);
    print_int("count", r.count);
    printf("}\n");
}

static void print_appsign(const T3AppSignResult& r) {
    printf("{\"success\":%s", r.success ? "true" : "false");
    print_str("error", r.error);
    print_str("msg", r.msg);
    print_str("autograph", r.autograph);
    print_int("time", (long)r.time);
    printf("}\n");
}

/* ========== main ========== */

static void usage() {
    printf(
        "用法: ./t3cli <命令> [参数...]\n"
        "\n"
        "  machine                               获取机器码\n"
        "  login <卡密>                           卡密登录（输出 statecode/endTime 等）\n"
        "  query <卡密>                           查询卡密\n"
        "  heartbeat <卡密> <statecode>           心跳（登录后每 30 秒）\n"
        "  notice                                获取程序公告\n"
        "  version                               获取最新版本号\n"
        "  update <当前版本号>                    检查更新\n"
        "  clouddoc <token>                      获取云文档\n"
        "  sign <待签名字符串>                    程序签名\n"
        "  register <用户名> <密码> [邮箱]        用户注册\n"
        "  userlogin <用户名> <密码>              用户登录\n"
        "  userheartbeat <用户名> <密码> <statecode>\n"
        "  change_password <用户名> <旧密码> <新密码>\n"
        "  user_cancel <用户名> <密码>            注销账号\n"
        "  recharge <用户名> <充值卡密>           用户充值\n"
        "  kami_recharge <目标卡密> <来源卡密>    卡密充值\n"
        "  unbind_kami <卡密> <机器码>            卡密解绑（换设备前用）\n"
        "  unbind_user <用户名> <密码> <机器码>   用户解绑\n"
        "  ip_unbind_kami <卡密>                  IP解绑卡密\n"
        "  ip_unbind_user <用户名> <密码>         IP解绑用户\n"
        "  disable_kami <卡密>                    禁用卡密\n"
        "  disable_user <用户名> <密码>           禁用用户\n"
        "  getvar_kami <卡密> <变量ID> <变量名>   按卡密读变量\n"
        "  getvar_user <用户名> <密码> <变量ID> <变量名>\n"
        "  modifyvar_kami <卡密> <变量ID> <内容>  按卡密改变量\n"
        "  modifyvar_user <用户名> <密码> <变量ID> <内容>\n"
        "  getcore_kami <卡密>                    按卡密读核心\n"
        "  getcore_user <用户名> <密码>           按用户读核心\n"
        "  modifycore_kami <卡密> <内容>          按卡密改核心\n"
        "  modifycore_user <用户名> <密码> <内容> 按用户改核心\n"
        "  online_kami                            在线卡密数\n"
        "  online_user                            在线用户数\n"
        "  heartbeat_any <statecode>              统一心跳\n"
        "\n"
        "所有命令输出 JSON。\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) { usage(); return 1; }

    T3Verify t3;
    if (!t3.initRSA(T3_LOGIN_CODE, T3_NOTICE_CODE, T3_VERSION_CODE, T3_HEARTBEAT_CODE,
                    T3_APPKEY, T3_RSA_PUBLIC_KEY)) {
        printf("{\"success\":false,\"error\":\"初始化失败，请检查 .so/凭证配置\"}\n");
        return 1;
    }
    for (const CodeEntry& e : EXTRA_CODES) t3.setCode(e.field, e.code);

    const std::string cmd = argv[1];
    std::vector<std::string> a;
    for (int i = 2; i < argc; i++) a.push_back(argv[i]);
    auto need = [&](size_t n) -> bool {
        if (a.size() >= n) return true;
        printf("{\"success\":false,\"error\":\"参数不足: %s 需要 %zu 个参数\"}\n", cmd.c_str(), n);
        return false;
    };

    if (cmd == "machine") {
        printf("{\"machine\":\"%s\"}\n", getMachineCode().c_str());
    } else if (cmd == "login" && need(1)) {
        print_login(t3.login(a[0], getMachineCode()));
    } else if (cmd == "query" && need(1)) {
        print_query(t3.queryKami(a[0]));
    } else if (cmd == "heartbeat" && need(2)) {
        print_result(t3.heartbeat(a[0], a[1]));
    } else if (cmd == "notice") {
        print_notice(t3.getNotice());
    } else if (cmd == "version") {
        print_version(t3.getLatestVersion());
    } else if (cmd == "update" && need(1)) {
        print_update(t3.checkUpdate(a[0]));
    } else if (cmd == "clouddoc" && need(1)) {
        print_clouddoc(t3.getCloudDoc(a[0]));
    } else if (cmd == "sign" && need(1)) {
        print_appsign(t3.appSign(a[0]));
    } else if (cmd == "register" && need(2)) {
        print_result(t3.userRegister(a[0], a[1], a.size() > 2 ? a[2] : ""));
    } else if (cmd == "userlogin" && need(2)) {
        print_login(t3.userLogin(a[0], a[1], getMachineCode()));
    } else if (cmd == "userheartbeat" && need(3)) {
        print_result(t3.userHeartbeat(a[0], a[1], a[2]));
    } else if (cmd == "change_password" && need(3)) {
        print_result(t3.changePassword(a[0], a[1], a[2]));
    } else if (cmd == "user_cancel" && need(2)) {
        print_result(t3.userCancel(a[0], a[1]));
    } else if (cmd == "recharge" && need(2)) {
        print_result(t3.recharge(a[0], a[1]));
    } else if (cmd == "kami_recharge" && need(2)) {
        print_result(t3.kamiRecharge(a[0], a[1]));
    } else if (cmd == "unbind_kami" && need(2)) {
        print_result(t3.unbindKami(a[0], a[1]));
    } else if (cmd == "unbind_user" && need(3)) {
        print_result(t3.unbindUser(a[0], a[1], a[2]));
    } else if (cmd == "ip_unbind_kami" && need(1)) {
        print_result(t3.ipUnbindKami(a[0]));
    } else if (cmd == "ip_unbind_user" && need(2)) {
        print_result(t3.ipUnbindUser(a[0], a[1]));
    } else if (cmd == "disable_kami" && need(1)) {
        print_result(t3.disableKami(a[0]));
    } else if (cmd == "disable_user" && need(2)) {
        print_result(t3.disableUser(a[0], a[1]));
    } else if (cmd == "getvar_kami" && need(3)) {
        print_variable(t3.getVariableByKami(a[0], a[1], a[2]));
    } else if (cmd == "getvar_user" && need(4)) {
        print_variable(t3.getVariableByUser(a[0], a[1], a[2], a[3]));
    } else if (cmd == "modifyvar_kami" && need(3)) {
        print_result(t3.modifyVariableByKami(a[0], a[1], a[2]));
    } else if (cmd == "modifyvar_user" && need(4)) {
        print_result(t3.modifyVariableByUser(a[0], a[1], a[2], a[3]));
    } else if (cmd == "getcore_kami" && need(1)) {
        print_core(t3.getCoreByKami(a[0]));
    } else if (cmd == "getcore_user" && need(2)) {
        print_core(t3.getCoreByUser(a[0], a[1]));
    } else if (cmd == "modifycore_kami" && need(2)) {
        print_result(t3.modifyCoreByKami(a[0], a[1]));
    } else if (cmd == "modifycore_user" && need(3)) {
        print_result(t3.modifyCoreByUser(a[0], a[1], a[2]));
    } else if (cmd == "online_kami") {
        print_online(t3.getOnlineKamiCount());
    } else if (cmd == "online_user") {
        print_online(t3.getOnlineUserCount());
    } else if (cmd == "heartbeat_any" && need(1)) {
        print_result(t3.heartbeatAny(a[0]));
    } else {
        printf("{\"success\":false,\"error\":\"未知命令: %s\"}\n", cmd.c_str());
        usage();
        return 1;
    }
    return 0;
}
