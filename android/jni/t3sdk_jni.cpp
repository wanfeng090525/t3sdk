/**
 * T3验证SDK - Android JNI 绑定层（精简版）
 *
 * 说明：
 * 1. 核心实现使用官方 T3Example_AndroidJNI 的 t3sdk.cpp（仅优化了 Android 机器码获取）
 * 2. 所有 T3 后台凭证（调用码/APPKEY/公钥）直接硬编码在下方宏中，编译进 libt3sdk.so
 * 3. Java 端不包含任何凭证，只通过 JNI 调用
 * 4. 只暴露登录所需的最小方法集：init / getMachineCode / login / heartbeat
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

/* ========== 初始化（凭证内置在 .so） ========== */

extern "C" JNIEXPORT jboolean JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeInit(JNIEnv *env, jobject thiz, jlong handle) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);
    if (!verify) return JNI_FALSE;
    /* RSA 模式（与官方示例 main.cpp 一致） */
    bool ok = verify->initRSA(
        T3_LOGIN_CODE, T3_NOTICE_CODE, T3_VERSION_CODE,
        T3_HEARTBEAT_CODE, T3_APPKEY, T3_RSA_PUBLIC_KEY);
    return ok ? JNI_TRUE : JNI_FALSE;
}

/* ========== 机器码（官方实现 + Android 优化） ========== */

extern "C" JNIEXPORT jstring JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeGetMachineCode(JNIEnv *env, jclass clazz) {
    std::string code = getMachineCode();
    return cpp_to_jstring(env, code);
}

/* ========== 登录 ========== */

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeLogin(
        JNIEnv *env, jobject thiz, jlong handle, jstring kami, jstring imei) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);

    /* 获取 Java 类 */
    jclass cls = env->FindClass("com/t3yanzheng/sdk/T3LoginResult");
    jmethodID ctor = env->GetMethodID(cls, "<init>", "()V");
    jobject obj = env->NewObject(cls, ctor);

    /* 字段 */
    jfieldID fSuccess  = env->GetFieldID(cls, "success", "Z");
    jfieldID fError    = env->GetFieldID(cls, "error", "Ljava/lang/String;");
    jfieldID fId       = env->GetFieldID(cls, "id", "Ljava/lang/String;");
    jfieldID fEndTime  = env->GetFieldID(cls, "endTime", "Ljava/lang/String;");
    jfieldID fStatecode= env->GetFieldID(cls, "statecode", "Ljava/lang/String;");
    jfieldID fRecharge = env->GetFieldID(cls, "recharge", "Ljava/lang/String;");
    jfieldID fUseTime  = env->GetFieldID(cls, "useTime", "Ljava/lang/String;");
    jfieldID fAmount   = env->GetFieldID(cls, "amount", "Ljava/lang/String;");
    jfieldID fAvailable= env->GetFieldID(cls, "available", "Ljava/lang/String;");
    jfieldID fImei     = env->GetFieldID(cls, "imei", "Ljava/lang/String;");
    jfieldID fChange   = env->GetFieldID(cls, "change", "Ljava/lang/String;");
    jfieldID fCore     = env->GetFieldID(cls, "core", "Ljava/lang/String;");

    if (!verify) return obj;

    T3LoginResult r = verify->login(jstring_to_cpp(env, kami), jstring_to_cpp(env, imei));

    env->SetBooleanField(obj, fSuccess, r.success ? JNI_TRUE : JNI_FALSE);
    env->SetObjectField(obj, fError,     cpp_to_jstring(env, r.error));
    env->SetObjectField(obj, fId,        cpp_to_jstring(env, r.id));
    env->SetObjectField(obj, fEndTime,   cpp_to_jstring(env, r.end_time));
    env->SetObjectField(obj, fStatecode, cpp_to_jstring(env, r.statecode));
    env->SetObjectField(obj, fRecharge,  cpp_to_jstring(env, r.recharge));
    env->SetObjectField(obj, fUseTime,   cpp_to_jstring(env, r.use_time));
    env->SetObjectField(obj, fAmount,    cpp_to_jstring(env, r.amount));
    env->SetObjectField(obj, fAvailable, cpp_to_jstring(env, r.available));
    env->SetObjectField(obj, fImei,      cpp_to_jstring(env, r.imei));
    env->SetObjectField(obj, fChange,    cpp_to_jstring(env, r.change));
    env->SetObjectField(obj, fCore,      cpp_to_jstring(env, r.core));

    return obj;
}

/* ========== 心跳 ========== */

extern "C" JNIEXPORT jobject JNICALL
Java_com_t3yanzheng_sdk_T3Verify_nativeHeartbeat(
        JNIEnv *env, jobject thiz, jlong handle, jstring kami, jstring statecode) {
    T3Verify *verify = reinterpret_cast<T3Verify *>(handle);

    jclass cls = env->FindClass("com/t3yanzheng/sdk/T3Result");
    jmethodID ctor = env->GetMethodID(cls, "<init>", "()V");
    jobject obj = env->NewObject(cls, ctor);

    jfieldID fSuccess = env->GetFieldID(cls, "success", "Z");
    jfieldID fError   = env->GetFieldID(cls, "error", "Ljava/lang/String;");
    jfieldID fMsg     = env->GetFieldID(cls, "msg", "Ljava/lang/String;");

    if (!verify) return obj;

    T3Result r = verify->heartbeat(jstring_to_cpp(env, kami), jstring_to_cpp(env, statecode));

    env->SetBooleanField(obj, fSuccess, r.success ? JNI_TRUE : JNI_FALSE);
    env->SetObjectField(obj, fError,    cpp_to_jstring(env, r.error));
    env->SetObjectField(obj, fMsg,      cpp_to_jstring(env, r.msg));

    return obj;
}
