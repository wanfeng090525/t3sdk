/**
 * T3验证SDK - Android JNI 绑定层（官方完整接口版）
 *
 * 说明：
 * 1. 核心实现使用官方 T3Example_AndroidJNI 的 t3sdk.cpp（仅优化了 Android 机器码获取）
 * 2. 所有 T3 后台凭证（调用码/APPKEY/公钥）直接硬编码在下方宏中，编译进 libt3sdk.so
 * 3. Java 端不包含任何凭证，只通过 JNI 调用
 * 4. 接口覆盖官方全部能力：卡密/用户/QQ/变量/核心数据/在线数量/公告/版本/更新/云文档/签名等
 */

#include <jni.h>
#include <string>
#include <cstring>
#include <cstdlib>
#include "t3sdk/t3sdk.h"

/* ============================================================
 * T3 后台凭证 - 直接编译进 .so，不会出现在 dex 中
 * 替换为你自己的 T3 后台配置后重新编译 .so 即可
 * 官网: https://www.t3yanzheng.com
 * ============================================================ */
#define T3_LOGIN_CODE     "813B2676E9690C89"    /* 单码登录调用码 */
#define T3_NOTICE_CODE    "EC56923E2FD91C99"    /* 获取程序公告调用码 */
#define T3_VERSION_CODE   "EA44543183C3F5D3"    /* 获取程序最新版本号调用码 */
#define T3_HEARTBEAT_CODE "9AB469F061FA45F4"    /* 单码卡密心跳验证调用码 */
#define T3_APPKEY         "d633f5e27c1b107cd2a1f98870787263"  /* 程序密钥APPKEY */
#define T3_RSA_PUBLIC_KEY "-----BEGIN PUBLIC KEY-----\n" \
                          "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDAQP0fmaGhF/sEskSVfDALBG2X\n" \
                          "KFCtn2HjJj0W+LQOL4bQIyg7Dh1lVUnTSodUwehXGloXHthU/c/Aio7xnYJILewg\n" \
                          "5QVYKjGbbexgO61KIg0AotYxV8KNUOAg8qPVfsQ+hELwJHAOFHfORSn/fZfd2hVg\n" \
                          "+YfzVfYS6KW/i0imOQIDAQAB\n" \
                          "-----END PUBLIC KEY-----"

/* ========== JNI 辅助 ========== */

static std::string jstring_to_cpp(JNIEnv *env, jstring js) {
    if (!js) return "";
    const char *chars = env->GetStringUTFChars(js, NULL);
    if (!chars) return "";
    std::string s(chars);
    env->ReleaseStringUTFChars(js, chars);
    return s;
}

static jstring cpp_to_jstring(JNIEnv *env, const std::string &s) {
    return env->NewStringUTF(s.c_str());
}

static jobject new_result(JNIEnv *env, const char *cls) {
    jclass c = env->FindClass(cls);
    if (!c) return NULL;
    jmethodID m = env->GetMethodID(c, "<init>", "()V");
    if (!m) return NULL;
    return env->NewObject(c, m);
}

static void set_bool(JNIEnv *env, jobject obj, const char *cls, const char *field, bool v) {
    jclass c = env->FindClass(cls);
    jfieldID f = env->GetFieldID(c, field, "Z");
    if (f) env->SetBooleanField(obj, f, v ? JNI_TRUE : JNI_FALSE);
}

static void set_int(JNIEnv *env, jobject obj, const char *cls, const char *field, jint v) {
    jclass c = env->FindClass(cls);
    jfieldID f = env->GetFieldID(c, field, "I");
    if (f) env->SetIntField(obj, f, v);
}

static void set_long(JNIEnv *env, jobject obj, const char *cls, const char *field, jlong v) {
    jclass c = env->FindClass(cls);
    jfieldID f = env->GetFieldID(c, field, "J");
    if (f) env->SetLongField(obj, f, v);
}

static void set_str(JNIEnv *env, jobject obj, const char *cls, const char *field, const std::string &v) {
    jclass c = env->FindClass(cls);
    jfieldID f = env->GetFieldID(c, field, "Ljava/lang/String;");
    if (f) env->SetObjectField(obj, f, cpp_to_jstring(env, v));
}

#define T3_RESULT_CLASS "com/t3yanzheng/sdk/T3Result"
#define T3_LOGIN_CLASS  "com/t3yanzheng/sdk/T3LoginResult"

/* ========== 生命周期 ========== */

extern "C" JNIEXPORT jlong JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeCreate(JNIEnv *env, jobject thiz) {
    T3Verify *verify = new T3Verify();
    return reinterpret_cast<jlong>(verify);
}

extern "C" JNIEXPORT void JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeDestroy(JNIEnv *env, jobject thiz, jlong handle) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    delete verify;
}

/* ========== 初始化（凭证内置在 .so，RSA 模式） ========== */

extern "C" JNIEXPORT jboolean JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeInit(JNIEnv *env, jobject thiz, jlong handle) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    if (!verify) return JNI_FALSE;
    bool ok = verify->initRSA(
        T3_LOGIN_CODE, T3_NOTICE_CODE, T3_VERSION_CODE,
        T3_HEARTBEAT_CODE, T3_APPKEY, T3_RSA_PUBLIC_KEY);
    return ok ? JNI_TRUE : JNI_FALSE;
}

/* ========== 动态调用码 ========== */

extern "C" JNIEXPORT void JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeSetCode(
        JNIEnv *env, jobject thiz, jlong handle, jstring field, jstring code) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    if (!verify) return;
    verify->setCode(jstring_to_cpp(env, field), jstring_to_cpp(env, code));
}

/* ========== 机器码（官方实现 + Android 优化） ========== */

extern "C" JNIEXPORT jstring JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeGetMachineCode(JNIEnv *env, jclass clazz) {
    return cpp_to_jstring(env, getMachineCode());
}

/* ========== 卡密验证 ========== */

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeLogin(
        JNIEnv *env, jobject thiz, jlong handle, jstring kami, jstring imei) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_LOGIN_CLASS);
    if (!verify || !obj) return obj;
    T3LoginResult r = verify->login(jstring_to_cpp(env, kami), jstring_to_cpp(env, imei));
    set_bool(env, obj, T3_LOGIN_CLASS, "success", r.success);
    set_str (env, obj, T3_LOGIN_CLASS, "error", r.error);
    set_str (env, obj, T3_LOGIN_CLASS, "id", r.id);
    set_str (env, obj, T3_LOGIN_CLASS, "endTime", r.end_time);
    set_str (env, obj, T3_LOGIN_CLASS, "statecode", r.statecode);
    set_str (env, obj, T3_LOGIN_CLASS, "recharge", r.recharge);
    set_str (env, obj, T3_LOGIN_CLASS, "useTime", r.use_time);
    set_str (env, obj, T3_LOGIN_CLASS, "amount", r.amount);
    set_str (env, obj, T3_LOGIN_CLASS, "available", r.available);
    set_str (env, obj, T3_LOGIN_CLASS, "imei", r.imei);
    set_str (env, obj, T3_LOGIN_CLASS, "change", r.change);
    set_str (env, obj, T3_LOGIN_CLASS, "core", r.core);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeQueryKami(
        JNIEnv *env, jobject thiz, jlong handle, jstring kami) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    const char *cls = "com/t3yanzheng/sdk/T3QueryResult";
    jobject obj = new_result(env, cls);
    if (!verify || !obj) return obj;
    T3QueryResult r = verify->queryKami(jstring_to_cpp(env, kami));
    set_bool(env, obj, cls, "success", r.success);
    set_str (env, obj, cls, "error", r.error);
    set_str (env, obj, cls, "state", r.state);
    set_str (env, obj, cls, "use", r.use);
    set_str (env, obj, cls, "id", r.id);
    set_str (env, obj, cls, "useTime", r.use_time);
    set_str (env, obj, cls, "endTime", r.end_time);
    set_str (env, obj, cls, "lineTime", r.line_time);
    set_str (env, obj, cls, "line", r.line);
    set_str (env, obj, cls, "amount", r.amount);
    set_str (env, obj, cls, "available", r.available);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeHeartbeat(
        JNIEnv *env, jobject thiz, jlong handle, jstring kami, jstring statecode) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->heartbeat(jstring_to_cpp(env, kami), jstring_to_cpp(env, statecode));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

/* ========== 数据与内容 ========== */

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeGetNotice(JNIEnv *env, jobject thiz, jlong handle) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    const char *cls = "com/t3yanzheng/sdk/T3NoticeResult";
    jobject obj = new_result(env, cls);
    if (!verify || !obj) return obj;
    T3NoticeResult r = verify->getNotice();
    set_bool(env, obj, cls, "success", r.success);
    set_str (env, obj, cls, "error", r.error);
    set_str (env, obj, cls, "notice", r.notice);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeGetLatestVersion(JNIEnv *env, jobject thiz, jlong handle) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    const char *cls = "com/t3yanzheng/sdk/T3VersionResult";
    jobject obj = new_result(env, cls);
    if (!verify || !obj) return obj;
    T3VersionResult r = verify->getLatestVersion();
    set_bool(env, obj, cls, "success", r.success);
    set_str (env, obj, cls, "error", r.error);
    set_str (env, obj, cls, "version", r.version);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeCheckUpdate(
        JNIEnv *env, jobject thiz, jlong handle, jstring ver) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    const char *cls = "com/t3yanzheng/sdk/T3UpdateResult";
    jobject obj = new_result(env, cls);
    if (!verify || !obj) return obj;
    T3UpdateResult r = verify->checkUpdate(jstring_to_cpp(env, ver));
    set_bool(env, obj, cls, "success", r.success);
    set_str (env, obj, cls, "error", r.error);
    set_bool(env, obj, cls, "hasUpdate", r.hasUpdate);
    set_str (env, obj, cls, "ver", r.ver);
    set_str (env, obj, cls, "version", r.version);
    set_str (env, obj, cls, "uplog", r.uplog);
    set_str (env, obj, cls, "upurl", r.upurl);
    set_str (env, obj, cls, "msg", r.msg);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeGetCloudDoc(
        JNIEnv *env, jobject thiz, jlong handle, jstring token) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    const char *cls = "com/t3yanzheng/sdk/T3CloudDocResult";
    jobject obj = new_result(env, cls);
    if (!verify || !obj) return obj;
    T3CloudDocResult r = verify->getCloudDoc(jstring_to_cpp(env, token));
    set_bool(env, obj, cls, "success", r.success);
    set_str (env, obj, cls, "error", r.error);
    set_str (env, obj, cls, "content", r.content);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeAppSign(
        JNIEnv *env, jobject thiz, jlong handle, jstring autograph) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    const char *cls = "com/t3yanzheng/sdk/T3AppSignResult";
    jobject obj = new_result(env, cls);
    if (!verify || !obj) return obj;
    T3AppSignResult r = verify->appSign(jstring_to_cpp(env, autograph));
    set_bool(env, obj, cls, "success", r.success);
    set_str (env, obj, cls, "error", r.error);
    set_str (env, obj, cls, "msg", r.msg);
    set_str (env, obj, cls, "autograph", r.autograph);
    set_long(env, obj, cls, "time", (jlong)r.time);
    return obj;
}

/* ========== 用户体系 ========== */

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeUserRegister(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring user, jstring pass, jstring email) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->userRegister(jstring_to_cpp(env, user), jstring_to_cpp(env, pass), jstring_to_cpp(env, email));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeUserLogin(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring user, jstring pass, jstring imei) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_LOGIN_CLASS);
    if (!verify || !obj) return obj;
    T3LoginResult r = verify->userLogin(jstring_to_cpp(env, user), jstring_to_cpp(env, pass), jstring_to_cpp(env, imei));
    set_bool(env, obj, T3_LOGIN_CLASS, "success", r.success);
    set_str (env, obj, T3_LOGIN_CLASS, "error", r.error);
    set_str (env, obj, T3_LOGIN_CLASS, "id", r.id);
    set_str (env, obj, T3_LOGIN_CLASS, "endTime", r.end_time);
    set_str (env, obj, T3_LOGIN_CLASS, "statecode", r.statecode);
    set_str (env, obj, T3_LOGIN_CLASS, "recharge", r.recharge);
    set_str (env, obj, T3_LOGIN_CLASS, "useTime", r.use_time);
    set_str (env, obj, T3_LOGIN_CLASS, "amount", r.amount);
    set_str (env, obj, T3_LOGIN_CLASS, "available", r.available);
    set_str (env, obj, T3_LOGIN_CLASS, "imei", r.imei);
    set_str (env, obj, T3_LOGIN_CLASS, "change", r.change);
    set_str (env, obj, T3_LOGIN_CLASS, "core", r.core);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeUserHeartbeat(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring user, jstring pass, jstring statecode) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->userHeartbeat(jstring_to_cpp(env, user), jstring_to_cpp(env, pass), jstring_to_cpp(env, statecode));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeQqLogin(
        JNIEnv *env, jobject thiz, jlong handle, jstring openid, jstring accessToken) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_LOGIN_CLASS);
    if (!verify || !obj) return obj;
    T3LoginResult r = verify->qqLogin(jstring_to_cpp(env, openid), jstring_to_cpp(env, accessToken));
    set_bool(env, obj, T3_LOGIN_CLASS, "success", r.success);
    set_str (env, obj, T3_LOGIN_CLASS, "error", r.error);
    set_str (env, obj, T3_LOGIN_CLASS, "id", r.id);
    set_str (env, obj, T3_LOGIN_CLASS, "endTime", r.end_time);
    set_str (env, obj, T3_LOGIN_CLASS, "statecode", r.statecode);
    set_str (env, obj, T3_LOGIN_CLASS, "recharge", r.recharge);
    set_str (env, obj, T3_LOGIN_CLASS, "useTime", r.use_time);
    set_str (env, obj, T3_LOGIN_CLASS, "amount", r.amount);
    set_str (env, obj, T3_LOGIN_CLASS, "available", r.available);
    set_str (env, obj, T3_LOGIN_CLASS, "imei", r.imei);
    set_str (env, obj, T3_LOGIN_CLASS, "change", r.change);
    set_str (env, obj, T3_LOGIN_CLASS, "core", r.core);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeBindQq(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring user, jstring pass, jstring openid, jstring accessToken) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->bindQQ(jstring_to_cpp(env, user), jstring_to_cpp(env, pass),
                                jstring_to_cpp(env, openid), jstring_to_cpp(env, accessToken));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeChangePassword(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring user, jstring oldpass, jstring newpass) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->changePassword(jstring_to_cpp(env, user), jstring_to_cpp(env, oldpass), jstring_to_cpp(env, newpass));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeUserCancel(
        JNIEnv *env, jobject thiz, jlong handle, jstring user, jstring pass) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->userCancel(jstring_to_cpp(env, user), jstring_to_cpp(env, pass));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeRecharge(
        JNIEnv *env, jobject thiz, jlong handle, jstring user, jstring card) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->recharge(jstring_to_cpp(env, user), jstring_to_cpp(env, card));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeKamiRecharge(
        JNIEnv *env, jobject thiz, jlong handle, jstring targetKami, jstring sourceKami) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->kamiRecharge(jstring_to_cpp(env, targetKami), jstring_to_cpp(env, sourceKami));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

/* ========== 设备与安全 ========== */

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeUnbindKami(
        JNIEnv *env, jobject thiz, jlong handle, jstring kami, jstring imei) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->unbindKami(jstring_to_cpp(env, kami), jstring_to_cpp(env, imei));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeUnbindUser(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring user, jstring pass, jstring imei) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->unbindUser(jstring_to_cpp(env, user), jstring_to_cpp(env, pass), jstring_to_cpp(env, imei));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeIpUnbindKami(
        JNIEnv *env, jobject thiz, jlong handle, jstring kami) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->ipUnbindKami(jstring_to_cpp(env, kami));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeIpUnbindUser(
        JNIEnv *env, jobject thiz, jlong handle, jstring user, jstring pass) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->ipUnbindUser(jstring_to_cpp(env, user), jstring_to_cpp(env, pass));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeDisableKami(
        JNIEnv *env, jobject thiz, jlong handle, jstring kami) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->disableKami(jstring_to_cpp(env, kami));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeDisableUser(
        JNIEnv *env, jobject thiz, jlong handle, jstring user, jstring pass) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->disableUser(jstring_to_cpp(env, user), jstring_to_cpp(env, pass));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

/* ========== 远程变量 ========== */

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeGetVariableByKami(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring kami, jstring valueid, jstring valuename) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    const char *cls = "com/t3yanzheng/sdk/T3VariableResult";
    jobject obj = new_result(env, cls);
    if (!verify || !obj) return obj;
    T3VariableResult r = verify->getVariableByKami(jstring_to_cpp(env, kami),
        jstring_to_cpp(env, valueid), jstring_to_cpp(env, valuename));
    set_bool(env, obj, cls, "success", r.success);
    set_str (env, obj, cls, "error", r.error);
    set_str (env, obj, cls, "value", r.value);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeGetVariableByUser(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring user, jstring pass, jstring valueid, jstring valuename) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    const char *cls = "com/t3yanzheng/sdk/T3VariableResult";
    jobject obj = new_result(env, cls);
    if (!verify || !obj) return obj;
    T3VariableResult r = verify->getVariableByUser(jstring_to_cpp(env, user),
        jstring_to_cpp(env, pass), jstring_to_cpp(env, valueid), jstring_to_cpp(env, valuename));
    set_bool(env, obj, cls, "success", r.success);
    set_str (env, obj, cls, "error", r.error);
    set_str (env, obj, cls, "value", r.value);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeModifyVariableByKami(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring kami, jstring valueid, jstring valuecontent) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->modifyVariableByKami(jstring_to_cpp(env, kami),
        jstring_to_cpp(env, valueid), jstring_to_cpp(env, valuecontent));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeModifyVariableByUser(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring user, jstring pass, jstring valueid, jstring valuecontent) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->modifyVariableByUser(jstring_to_cpp(env, user),
        jstring_to_cpp(env, pass), jstring_to_cpp(env, valueid), jstring_to_cpp(env, valuecontent));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

/* ========== 核心数据 ========== */

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeModifyCoreByKami(
        JNIEnv *env, jobject thiz, jlong handle, jstring kami, jstring core) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->modifyCoreByKami(jstring_to_cpp(env, kami), jstring_to_cpp(env, core));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeModifyCoreByUser(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring user, jstring pass, jstring core) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->modifyCoreByUser(jstring_to_cpp(env, user), jstring_to_cpp(env, pass), jstring_to_cpp(env, core));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeGetCoreByKami(
        JNIEnv *env, jobject thiz, jlong handle, jstring kami) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    const char *cls = "com/t3yanzheng/sdk/T3CoreResult";
    jobject obj = new_result(env, cls);
    if (!verify || !obj) return obj;
    T3CoreResult r = verify->getCoreByKami(jstring_to_cpp(env, kami));
    set_bool(env, obj, cls, "success", r.success);
    set_str (env, obj, cls, "error", r.error);
    set_str (env, obj, cls, "core", r.core);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeGetCoreByUser(
        JNIEnv *env, jobject thiz, jlong handle, jstring user, jstring pass) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    const char *cls = "com/t3yanzheng/sdk/T3CoreResult";
    jobject obj = new_result(env, cls);
    if (!verify || !obj) return obj;
    T3CoreResult r = verify->getCoreByUser(jstring_to_cpp(env, user), jstring_to_cpp(env, pass));
    set_bool(env, obj, cls, "success", r.success);
    set_str (env, obj, cls, "error", r.error);
    set_str (env, obj, cls, "core", r.core);
    return obj;
}

/* ========== 在线数量 ========== */

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeGetOnlineKamiCount(JNIEnv *env, jobject thiz, jlong handle) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    const char *cls = "com/t3yanzheng/sdk/T3OnlineResult";
    jobject obj = new_result(env, cls);
    if (!verify || !obj) return obj;
    T3OnlineResult r = verify->getOnlineKamiCount();
    set_bool(env, obj, cls, "success", r.success);
    set_str (env, obj, cls, "error", r.error);
    set_int (env, obj, cls, "count", r.count);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeGetOnlineUserCount(JNIEnv *env, jobject thiz, jlong handle) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    const char *cls = "com/t3yanzheng/sdk/T3OnlineResult";
    jobject obj = new_result(env, cls);
    if (!verify || !obj) return obj;
    T3OnlineResult r = verify->getOnlineUserCount();
    set_bool(env, obj, cls, "success", r.success);
    set_str (env, obj, cls, "error", r.error);
    set_int (env, obj, cls, "count", r.count);
    return obj;
}

/* ========== QQ凭证 + 统一心跳 ========== */

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeGetVariableByQq(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring openid, jstring accessToken, jstring valueid, jstring valuename) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    const char *cls = "com/t3yanzheng/sdk/T3VariableResult";
    jobject obj = new_result(env, cls);
    if (!verify || !obj) return obj;
    T3VariableResult r = verify->getVariableByQq(jstring_to_cpp(env, openid),
        jstring_to_cpp(env, accessToken), jstring_to_cpp(env, valueid), jstring_to_cpp(env, valuename));
    set_bool(env, obj, cls, "success", r.success);
    set_str (env, obj, cls, "error", r.error);
    set_str (env, obj, cls, "value", r.value);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeQqHeartbeat(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring openid, jstring accessToken, jstring statecode) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->qqHeartbeat(jstring_to_cpp(env, openid), jstring_to_cpp(env, accessToken), jstring_to_cpp(env, statecode));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeGetUserCoreByQq(
        JNIEnv *env, jobject thiz, jlong handle, jstring openid, jstring accessToken) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    const char *cls = "com/t3yanzheng/sdk/T3CoreResult";
    jobject obj = new_result(env, cls);
    if (!verify || !obj) return obj;
    T3CoreResult r = verify->getUserCoreByQq(jstring_to_cpp(env, openid), jstring_to_cpp(env, accessToken));
    set_bool(env, obj, cls, "success", r.success);
    set_str (env, obj, cls, "error", r.error);
    set_str (env, obj, cls, "core", r.core);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeModifyUserCoreByQq(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring openid, jstring accessToken, jstring core) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->modifyUserCoreByQq(jstring_to_cpp(env, openid), jstring_to_cpp(env, accessToken), jstring_to_cpp(env, core));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeUnbindQq(
        JNIEnv *env, jobject thiz, jlong handle, jstring user, jstring pass) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->unbindQq(jstring_to_cpp(env, user), jstring_to_cpp(env, pass));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeHeartbeatAny(
        JNIEnv *env, jobject thiz, jlong handle, jstring statecode) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    jobject obj = new_result(env, T3_RESULT_CLASS);
    if (!verify || !obj) return obj;
    T3Result r = verify->heartbeatAny(jstring_to_cpp(env, statecode));
    set_bool(env, obj, T3_RESULT_CLASS, "success", r.success);
    set_str (env, obj, T3_RESULT_CLASS, "error", r.error);
    set_str (env, obj, T3_RESULT_CLASS, "msg", r.msg);
    return obj;
}
