/**
 * T3 验证 SDK - JNI 绑定层
 *
 * 包名: com.t3yanzheng.sdk
 * 将 C SDK 的接口暴露给 Java/Kotlin 调用
 */

#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "t3sdk.h"

/* ============================================================
 * T3 后台凭证 - 直接编译进 .so，不在 Java/dex 中出现
 * 替换为你自己的 T3 后台配置后重新编译 .so 即可
 * 官网: https://www.t3yanzheng.com
 * ============================================================ */
#define T3_LOGIN_CODE     "813B2676E9690C89"    /* 登录调用码 */
#define T3_NOTICE_CODE    "EC56923E2FD91C99"    /* 公告调用码 */
#define T3_VERSION_CODE   "EA44543183C3F5D3"    /* 版本调用码 */
#define T3_HEARTBEAT_CODE "9AB469F061FA45F4"    /* 心跳调用码 */
#define T3_APPKEY         "d633f5e27c1b107cd2a1f98870787263"  /* APPKEY */

/* Base64 自定义字符集模式（默认） - 64 个字符 */
#define T3_BASE64_CHARSET "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"

/* RSA 模式公钥（如后台使用 RSA 加密则取消注释并填入你的公钥） */
#define T3_RSA_PUBLIC_KEY "-----BEGIN PUBLIC KEY-----\n" \
                          "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDAQP0fmaGhF/sEskSVfDALBG2X\n" \
                          "KFCtn2HjJj0W+LQOL4bQIyg7Dh1lVUnTSodUwehXGloXHthU/c/Aio7xnYJILewg\n" \
                          "5QVYKjGbbexgO61KIg0AotYxV8KNUOAg8qPVfsQ+hELwJHAOFHfORSn/fZfd2hVg\n" \
                          "+YfzVfYS6KW/i0imOQIDAQAB\n" \
                          "-----END PUBLIC KEY-----"

/* ========== 工具函数 ========== */

static char *jstring_to_cstr(JNIEnv *env, jstring jstr) {
    if (jstr == NULL) return NULL;
    const char *utf = (*env)->GetStringUTFChars(env, jstr, NULL);
    if (utf == NULL) return NULL;
    char *result = strdup(utf);
    (*env)->ReleaseStringUTFChars(env, jstr, utf);
    return result;
}

static jstring cstr_to_jstring(JNIEnv *env, const char *str) {
    if (str == NULL) str = "";
    return (*env)->NewStringUTF(env, str);
}

static jlong cptr_to_jlong(void *ptr) {
    return (jlong)(intptr_t)ptr;
}

static void *jlong_to_cptr(jlong handle) {
    return (void *)(intptr_t)handle;
}

/* ========== 创建 Java 结果对象 ========== */

static jobject create_result_object(JNIEnv *env, const char *class_name,
                                    jboolean success, const char *error, const char *msg) {
    jclass cls = (*env)->FindClass(env, class_name);
    if (cls == NULL) return NULL;
    jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    if (ctor == NULL) return NULL;
    jobject obj = (*env)->NewObject(env, cls, ctor);

    jfieldID f_success = (*env)->GetFieldID(env, cls, "success", "Z");
    jfieldID f_error   = (*env)->GetFieldID(env, cls, "error", "Ljava/lang/String;");
    jfieldID f_msg     = (*env)->GetFieldID(env, cls, "msg", "Ljava/lang/String;");

    if (f_success) (*env)->SetBooleanField(env, obj, f_success, success);
    if (f_error)   (*env)->SetObjectField(env, obj, f_error, cstr_to_jstring(env, error));
    if (f_msg)     (*env)->SetObjectField(env, obj, f_msg, cstr_to_jstring(env, msg));

    return obj;
}

/* ========== 结果对象辅助函数 ========== */

static jobject create_login_result(JNIEnv *env, int ret, const T3LoginResult *result) {
    jclass cls = (*env)->FindClass(env, "com/t3yanzheng/sdk/T3LoginResult");
    if (cls == NULL) return NULL;
    jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, ctor);

    jfieldID f;
    f = (*env)->GetFieldID(env, cls, "success", "Z");
    if (f) (*env)->SetBooleanField(env, obj, f, ret == 0 ? JNI_TRUE : JNI_FALSE);
    f = (*env)->GetFieldID(env, cls, "error", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result->error));
    f = (*env)->GetFieldID(env, cls, "kami", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result->kami));
    f = (*env)->GetFieldID(env, cls, "id", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result->id));
    f = (*env)->GetFieldID(env, cls, "endTime", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result->end_time));
    f = (*env)->GetFieldID(env, cls, "statecode", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result->statecode));
    f = (*env)->GetFieldID(env, cls, "recharge", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result->recharge));
    f = (*env)->GetFieldID(env, cls, "useTime", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result->use_time));
    f = (*env)->GetFieldID(env, cls, "amount", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result->amount));
    f = (*env)->GetFieldID(env, cls, "available", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result->available));
    f = (*env)->GetFieldID(env, cls, "imei", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result->imei));
    f = (*env)->GetFieldID(env, cls, "change", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result->change));
    f = (*env)->GetFieldID(env, cls, "core", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result->core));

    return obj;
}

/* ============================================================
 * 自动登录（在 .so 内实现，不依赖 Java 业务逻辑）
 *
 * 卡密保存到应用私有目录下的 t3_saved_card.dat 文件中：
 *  - 登录成功时自动写入（nativeLogin 内完成）
 *  - 自动登录时读取并验证，失败自动清除
 *  - 机器码在 .so 内获取，Java 端只需 setStoragePath + autoLogin
 * ============================================================ */
static char g_storage_dir[1024] = {0};  /* 应用私有目录，由 Java 端传入 */
static const char *kSavedCardFile = "t3_saved_card.dat";

static void saved_card_path(char *out, size_t size) {
    out[0] = '\0';
    if (g_storage_dir[0] == '\0') return;
    snprintf(out, size, "%s/%s", g_storage_dir, kSavedCardFile);
}

static void save_card_file(const char *kami) {
    if (g_storage_dir[0] == '\0' || kami == NULL || kami[0] == '\0') return;
    char path[1100];
    saved_card_path(path, sizeof(path));
    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(kami, 1, strlen(kami), f);
        fclose(f);
    }
}

static void load_card_file(char *out, size_t size) {
    out[0] = '\0';
    if (g_storage_dir[0] == '\0') return;
    char path[1100];
    saved_card_path(path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) return;
    size_t n = fread(out, 1, size - 1, f);
    out[n] = '\0';
    fclose(f);
}

static void clear_card_file(void) {
    if (g_storage_dir[0] == '\0') return;
    char path[1100];
    saved_card_path(path, sizeof(path));
    remove(path);
}

/* 机器码（与 nativeGetMachineCode 相同逻辑） */
static void native_get_machine_code(char *out, size_t size) {
    if (get_machine_code(out) != 0 || strlen(out) == 0) {
        md5_string_upper("00:00:00:00:00:00", out);
    }
}

static jobject create_variable_result(JNIEnv *env, int ret, const T3VariableResult *result) {
    jclass cls = (*env)->FindClass(env, "com/t3yanzheng/sdk/T3VariableResult");
    if (cls == NULL) return NULL;
    jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, ctor);

    jfieldID f;
    f = (*env)->GetFieldID(env, cls, "success", "Z");
    if (f) (*env)->SetBooleanField(env, obj, f, ret == 0 ? JNI_TRUE : JNI_FALSE);
    f = (*env)->GetFieldID(env, cls, "error", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result->error));
    f = (*env)->GetFieldID(env, cls, "value", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result->value));

    return obj;
}

static jobject create_core_result(JNIEnv *env, int ret, const T3CoreResult *result) {
    jclass cls = (*env)->FindClass(env, "com/t3yanzheng/sdk/T3CoreResult");
    if (cls == NULL) return NULL;
    jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, ctor);

    jfieldID f;
    f = (*env)->GetFieldID(env, cls, "success", "Z");
    if (f) (*env)->SetBooleanField(env, obj, f, ret == 0 ? JNI_TRUE : JNI_FALSE);
    f = (*env)->GetFieldID(env, cls, "error", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result->error));
    f = (*env)->GetFieldID(env, cls, "core", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result->core));

    return obj;
}

/* ========== Native 方法实现 ========== */

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeCreate
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeCreate(JNIEnv *env, jobject thiz) {
    T3Verify *verify = (T3Verify *)calloc(1, sizeof(T3Verify));
    return cptr_to_jlong(verify);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeDestroy
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeDestroy(JNIEnv *env, jobject thiz, jlong handle) {
    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    if (verify) free(verify);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeInit
 * Signature: (J)Z
 * 凭证直接编译在 .so 中（见文件顶部 T3_* 宏）
 * 默认使用 RSA 模式（与官方示例 main.c 一致）
 */
JNIEXPORT jboolean JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeInit(
        JNIEnv *env, jobject thiz, jlong handle) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    if (!verify) return JNI_FALSE;

    int ret = t3verify_init_rsa(verify,
        T3_LOGIN_CODE, T3_NOTICE_CODE, T3_VERSION_CODE,
        T3_HEARTBEAT_CODE, T3_APPKEY, T3_RSA_PUBLIC_KEY);
    if (ret != 0) return JNI_FALSE;

    /* 设置新增接口调用码（与官方 example_all_api.c 保持一致） */
    t3verify_set_code(verify, "query",            "A2AC50154CC51A76");
    t3verify_set_code(verify, "register",         "6B9EABFF00B80750");
    t3verify_set_code(verify, "user_login",       "ECCFEFE957984480");
    t3verify_set_code(verify, "user_heartbeat",   "EF796F4016C97A66");
    t3verify_set_code(verify, "qq_login",         "B6652EB5C78F0641");
    t3verify_set_code(verify, "bind_qq",          "BF0378334FF0F0F5");
    t3verify_set_code(verify, "change_password",  "B7C4CD0FAE40AE8C");
    t3verify_set_code(verify, "user_cancel",      "A71666D3D237E922");
    t3verify_set_code(verify, "recharge",         "57559AB00DE6A9DE");
    t3verify_set_code(verify, "kami_recharge",    "A4634B5AC1F5341A");
    t3verify_set_code(verify, "unbind",           "2CA5F5986D945633");
    t3verify_set_code(verify, "ip_unbind",        "C21B2219566669B9");
    t3verify_set_code(verify, "disable",          "CF41580160F0AE75");
    t3verify_set_code(verify, "check_update",     "890829CBEFA61A56");
    t3verify_set_code(verify, "get_variable",     "A015ABD9403AC64A");
    t3verify_set_code(verify, "modify_variable",  "78CFB294B32DC2A3");
    t3verify_set_code(verify, "modify_core",      "F9CE457547B9DD0F");
    t3verify_set_code(verify, "get_kami_core",    "8C58C52B3053820A");
    t3verify_set_code(verify, "get_user_core",    "CD79FE55F3CE48B2");
    t3verify_set_code(verify, "online_kami",      "B61D29EBFA5E3CF5");
    t3verify_set_code(verify, "online_user",      "0D46101793B42DD3");
    t3verify_set_code(verify, "cloud_doc",        "F50B362FC86CC38E");
    t3verify_set_code(verify, "app_sign",         "BAB98433A890FFB0");
    t3verify_set_code(verify, "qq_heartbeat",     "77C9731065BAB2A4");
    t3verify_set_code(verify, "qq_get_variable",  "2CE5BD9B12733032");
    t3verify_set_code(verify, "qq_get_core",      "5EF62F8A7630DC9A");
    t3verify_set_code(verify, "qq_modify_core",   "ACA861E2C03E9385");
    t3verify_set_code(verify, "qq_unbind",        "8E113021CB8FE80E");
    t3verify_set_code(verify, "heartbeat_any",    "A0D682FB88F82D31");

    return JNI_TRUE;
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeInitRSA
 * Signature: (J)Z
 * RSA 模式，凭证和公钥直接编译在 .so 中
 */
JNIEXPORT jboolean JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeInitRSA(
        JNIEnv *env, jobject thiz, jlong handle) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    if (!verify) return JNI_FALSE;

    int ret = t3verify_init_rsa(verify,
        T3_LOGIN_CODE, T3_NOTICE_CODE, T3_VERSION_CODE,
        T3_HEARTBEAT_CODE, T3_APPKEY, T3_RSA_PUBLIC_KEY);
    if (ret != 0) return JNI_FALSE;

    /* 设置新增接口调用码（与官方 example_all_api.c 保持一致） */
    t3verify_set_code(verify, "query",            "A2AC50154CC51A76");
    t3verify_set_code(verify, "register",         "6B9EABFF00B80750");
    t3verify_set_code(verify, "user_login",       "ECCFEFE957984480");
    t3verify_set_code(verify, "user_heartbeat",   "EF796F4016C97A66");
    t3verify_set_code(verify, "qq_login",         "B6652EB5C78F0641");
    t3verify_set_code(verify, "bind_qq",          "BF0378334FF0F0F5");
    t3verify_set_code(verify, "change_password",  "B7C4CD0FAE40AE8C");
    t3verify_set_code(verify, "user_cancel",      "A71666D3D237E922");
    t3verify_set_code(verify, "recharge",         "57559AB00DE6A9DE");
    t3verify_set_code(verify, "kami_recharge",    "A4634B5AC1F5341A");
    t3verify_set_code(verify, "unbind",           "2CA5F5986D945633");
    t3verify_set_code(verify, "ip_unbind",        "C21B2219566669B9");
    t3verify_set_code(verify, "disable",          "CF41580160F0AE75");
    t3verify_set_code(verify, "check_update",     "890829CBEFA61A56");
    t3verify_set_code(verify, "get_variable",     "A015ABD9403AC64A");
    t3verify_set_code(verify, "modify_variable",  "78CFB294B32DC2A3");
    t3verify_set_code(verify, "modify_core",      "F9CE457547B9DD0F");
    t3verify_set_code(verify, "get_kami_core",    "8C58C52B3053820A");
    t3verify_set_code(verify, "get_user_core",    "CD79FE55F3CE48B2");
    t3verify_set_code(verify, "online_kami",      "B61D29EBFA5E3CF5");
    t3verify_set_code(verify, "online_user",      "0D46101793B42DD3");
    t3verify_set_code(verify, "cloud_doc",        "F50B362FC86CC38E");
    t3verify_set_code(verify, "app_sign",         "BAB98433A890FFB0");
    t3verify_set_code(verify, "qq_heartbeat",     "77C9731065BAB2A4");
    t3verify_set_code(verify, "qq_get_variable",  "2CE5BD9B12733032");
    t3verify_set_code(verify, "qq_get_core",      "5EF62F8A7630DC9A");
    t3verify_set_code(verify, "qq_modify_core",   "ACA861E2C03E9385");
    t3verify_set_code(verify, "qq_unbind",        "8E113021CB8FE80E");
    t3verify_set_code(verify, "heartbeat_any",    "A0D682FB88F82D31");

    return JNI_TRUE;
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeSetCode
 */
JNIEXPORT void JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeSetCode(
        JNIEnv *env, jobject thiz, jlong handle, jstring field, jstring code) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    if (!verify) return;

    char *f = jstring_to_cstr(env, field);
    char *c = jstring_to_cstr(env, code);
    t3verify_set_code(verify, f, c);
    free(f); free(c);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeGetMachineCode
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeGetMachineCode(JNIEnv *env, jclass clazz) {
    char machine_code[64] = {0};
    if (get_machine_code(machine_code) != 0 || strlen(machine_code) == 0) {
        /* 兜底：获取失败时使用官方默认机器码（00:00:00:00:00:00 的 MD5），
         * 与官方 C++ 版行为一致，保证机器码永不为空 */
        md5_string_upper("00:00:00:00:00:00", machine_code);
    }
    return cstr_to_jstring(env, machine_code);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeLogin
 * Signature: (JLjava/lang/String;Ljava/lang/String;)Lcom/t3yanzheng/sdk/T3LoginResult;
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeLogin(
        JNIEnv *env, jobject thiz, jlong handle, jstring kami, jstring imei) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3LoginResult result;
    memset(&result, 0, sizeof(result));

    char *k = jstring_to_cstr(env, kami);
    char *i = jstring_to_cstr(env, imei);

    int ret = t3verify_login(verify, k ? k : "", i ? i : "", &result);

    /* 回填本次登录使用的卡密（供 Java 端保存/展示） */
    if (k) {
        strncpy(result.kami, k, MAX_KAMI_LEN - 1);
        result.kami[MAX_KAMI_LEN - 1] = '\0';
    }

    /* 登录成功自动保存卡密，供下次自动登录（.so 内完成） */
    if (ret == 0 && k && k[0] != '\0') save_card_file(k);

    free(k); free(i);

    jclass cls = (*env)->FindClass(env, "com/t3yanzheng/sdk/T3LoginResult");
    if (cls == NULL) return NULL;
    jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, ctor);

    jfieldID f;
    f = (*env)->GetFieldID(env, cls, "success", "Z");
    if (f) (*env)->SetBooleanField(env, obj, f, ret == 0 ? JNI_TRUE : JNI_FALSE);
    f = (*env)->GetFieldID(env, cls, "error", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.error));
    f = (*env)->GetFieldID(env, cls, "kami", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.kami));
    f = (*env)->GetFieldID(env, cls, "id", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.id));
    f = (*env)->GetFieldID(env, cls, "endTime", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.end_time));
    f = (*env)->GetFieldID(env, cls, "statecode", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.statecode));
    f = (*env)->GetFieldID(env, cls, "recharge", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.recharge));
    f = (*env)->GetFieldID(env, cls, "useTime", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.use_time));
    f = (*env)->GetFieldID(env, cls, "amount", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.amount));
    f = (*env)->GetFieldID(env, cls, "available", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.available));
    f = (*env)->GetFieldID(env, cls, "imei", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.imei));
    f = (*env)->GetFieldID(env, cls, "change", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.change));
    f = (*env)->GetFieldID(env, cls, "core", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.core));

    return obj;
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeSetStoragePath
 * Signature: (JLjava/lang/String;)V
 * 自动登录 - 设置卡密存储目录（应用私有目录）
 */
JNIEXPORT void JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeSetStoragePath(
        JNIEnv *env, jobject thiz, jlong handle, jstring path) {
    char *p = jstring_to_cstr(env, path);
    if (p) {
        strncpy(g_storage_dir, p, sizeof(g_storage_dir) - 1);
        g_storage_dir[sizeof(g_storage_dir) - 1] = '\0';
        free(p);
    }
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeSaveCard
 * Signature: (JLjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeSaveCard(
        JNIEnv *env, jobject thiz, jlong handle, jstring kami) {
    char *k = jstring_to_cstr(env, kami);
    if (k) {
        save_card_file(k);
        free(k);
    }
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeClearSavedCard
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeClearSavedCard(
        JNIEnv *env, jobject thiz, jlong handle) {
    clear_card_file();
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeHasSavedCard
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeHasSavedCard(
        JNIEnv *env, jobject thiz, jlong handle) {
    char card[256] = {0};
    load_card_file(card, sizeof(card));
    return card[0] == '\0' ? JNI_FALSE : JNI_TRUE;
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeAutoLogin
 * Signature: (J)Lcom/t3yanzheng/sdk/T3LoginResult;
 * 自动登录（.so 内实现，机器码也在 .so 内获取）
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeAutoLogin(
        JNIEnv *env, jobject thiz, jlong handle) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3LoginResult result;
    memset(&result, 0, sizeof(result));

    char card[256] = {0};
    load_card_file(card, sizeof(card));
    if (card[0] == '\0') {
        return create_login_result(env, 1, &result);  /* 无已保存卡密 */
    }
    /* 回填本次登录使用的卡密 */
    strncpy(result.kami, card, MAX_KAMI_LEN - 1);
    result.kami[MAX_KAMI_LEN - 1] = '\0';

    char machine[64] = {0};
    native_get_machine_code(machine, sizeof(machine));

    int ret = t3verify_login(verify, card, machine, &result);

    /* 官方自动登录逻辑：失败时清除保存的卡密，下次手动输入 */
    if (ret != 0) clear_card_file();

    return create_login_result(env, ret, &result);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeHeartbeat
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeHeartbeat(
        JNIEnv *env, jobject thiz, jlong handle, jstring kami, jstring statecode) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *k = jstring_to_cstr(env, kami);
    char *s = jstring_to_cstr(env, statecode);

    t3verify_heartbeat(verify, k ? k : "", s ? s : "", &result);
    free(k); free(s);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeGetNotice
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeGetNotice(
        JNIEnv *env, jobject thiz, jlong handle) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3NoticeResult result;
    memset(&result, 0, sizeof(result));

    int ret = t3verify_get_notice(verify, &result);

    jclass cls = (*env)->FindClass(env, "com/t3yanzheng/sdk/T3NoticeResult");
    if (cls == NULL) return NULL;
    jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, ctor);

    jfieldID f;
    f = (*env)->GetFieldID(env, cls, "success", "Z");
    if (f) (*env)->SetBooleanField(env, obj, f, ret == 0 ? JNI_TRUE : JNI_FALSE);
    f = (*env)->GetFieldID(env, cls, "error", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.error));
    f = (*env)->GetFieldID(env, cls, "notice", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.notice));

    return obj;
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeGetLatestVersion
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeGetLatestVersion(
        JNIEnv *env, jobject thiz, jlong handle) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3VersionResult result;
    memset(&result, 0, sizeof(result));

    int ret = t3verify_get_latest_version(verify, &result);

    jclass cls = (*env)->FindClass(env, "com/t3yanzheng/sdk/T3VersionResult");
    if (cls == NULL) return NULL;
    jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, ctor);

    jfieldID f;
    f = (*env)->GetFieldID(env, cls, "success", "Z");
    if (f) (*env)->SetBooleanField(env, obj, f, ret == 0 ? JNI_TRUE : JNI_FALSE);
    f = (*env)->GetFieldID(env, cls, "error", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.error));
    f = (*env)->GetFieldID(env, cls, "version", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.version));

    return obj;
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeQueryKami
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeQueryKami(
        JNIEnv *env, jobject thiz, jlong handle, jstring kami) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3QueryResult result;
    memset(&result, 0, sizeof(result));

    char *k = jstring_to_cstr(env, kami);
    int ret = t3verify_query_kami(verify, k ? k : "", &result);
    free(k);

    jclass cls = (*env)->FindClass(env, "com/t3yanzheng/sdk/T3QueryResult");
    if (cls == NULL) return NULL;
    jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, ctor);

    jfieldID f;
    f = (*env)->GetFieldID(env, cls, "success", "Z");
    if (f) (*env)->SetBooleanField(env, obj, f, ret == 0 ? JNI_TRUE : JNI_FALSE);
    f = (*env)->GetFieldID(env, cls, "error", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.error));
    f = (*env)->GetFieldID(env, cls, "state", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.state));
    f = (*env)->GetFieldID(env, cls, "use", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.use));
    f = (*env)->GetFieldID(env, cls, "id", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.id));
    f = (*env)->GetFieldID(env, cls, "useTime", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.use_time));
    f = (*env)->GetFieldID(env, cls, "endTime", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.end_time));
    f = (*env)->GetFieldID(env, cls, "lineTime", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.line_time));
    f = (*env)->GetFieldID(env, cls, "line", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.line));
    f = (*env)->GetFieldID(env, cls, "amount", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.amount));
    f = (*env)->GetFieldID(env, cls, "available", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.available));

    return obj;
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeCheckUpdate
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeCheckUpdate(
        JNIEnv *env, jobject thiz, jlong handle, jstring ver) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3UpdateResult result;
    memset(&result, 0, sizeof(result));

    char *v = jstring_to_cstr(env, ver);
    int ret = t3verify_check_update(verify, v ? v : "", &result);
    free(v);

    jclass cls = (*env)->FindClass(env, "com/t3yanzheng/sdk/T3UpdateResult");
    if (cls == NULL) return NULL;
    jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, ctor);

    jfieldID f;
    f = (*env)->GetFieldID(env, cls, "success", "Z");
    if (f) (*env)->SetBooleanField(env, obj, f, ret == 0 ? JNI_TRUE : JNI_FALSE);
    f = (*env)->GetFieldID(env, cls, "error", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.error));
    f = (*env)->GetFieldID(env, cls, "hasUpdate", "Z");
    if (f) (*env)->SetBooleanField(env, obj, f, result.has_update ? JNI_TRUE : JNI_FALSE);
    f = (*env)->GetFieldID(env, cls, "ver", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.ver));
    f = (*env)->GetFieldID(env, cls, "version", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.version));
    f = (*env)->GetFieldID(env, cls, "uplog", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.uplog));
    f = (*env)->GetFieldID(env, cls, "upurl", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.upurl));
    f = (*env)->GetFieldID(env, cls, "msg", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.msg));

    return obj;
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeUserLogin
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeUserLogin(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring user, jstring pass, jstring imei) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3LoginResult result;
    memset(&result, 0, sizeof(result));

    char *u = jstring_to_cstr(env, user);
    char *p = jstring_to_cstr(env, pass);
    char *i = jstring_to_cstr(env, imei);

    int ret = t3verify_user_login(verify, u ? u : "", p ? p : "", i ? i : "", &result);
    free(u); free(p); free(i);

    jclass cls = (*env)->FindClass(env, "com/t3yanzheng/sdk/T3LoginResult");
    if (cls == NULL) return NULL;
    jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, ctor);

    jfieldID f;
    f = (*env)->GetFieldID(env, cls, "success", "Z");
    if (f) (*env)->SetBooleanField(env, obj, f, ret == 0 ? JNI_TRUE : JNI_FALSE);
    f = (*env)->GetFieldID(env, cls, "error", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.error));
    f = (*env)->GetFieldID(env, cls, "id", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.id));
    f = (*env)->GetFieldID(env, cls, "endTime", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.end_time));
    f = (*env)->GetFieldID(env, cls, "statecode", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.statecode));
    f = (*env)->GetFieldID(env, cls, "core", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.core));

    return obj;
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeUserRegister
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeUserRegister(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring user, jstring pass, jstring email) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *u = jstring_to_cstr(env, user);
    char *p = jstring_to_cstr(env, pass);
    char *e = jstring_to_cstr(env, email);

    t3verify_user_register(verify, u ? u : "", p ? p : "", e ? e : "", &result);
    free(u); free(p); free(e);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeUserHeartbeat
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeUserHeartbeat(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring user, jstring pass, jstring statecode) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *u = jstring_to_cstr(env, user);
    char *p = jstring_to_cstr(env, pass);
    char *s = jstring_to_cstr(env, statecode);

    t3verify_user_heartbeat(verify, u ? u : "", p ? p : "", s ? s : "", &result);
    free(u); free(p); free(s);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeHeartbeatAny
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeHeartbeatAny(
        JNIEnv *env, jobject thiz, jlong handle, jstring statecode) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *s = jstring_to_cstr(env, statecode);
    t3verify_heartbeat_any(verify, s ? s : "", &result);
    free(s);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeGetOnlineKamiCount
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeGetOnlineKamiCount(
        JNIEnv *env, jobject thiz, jlong handle) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3OnlineResult result;
    memset(&result, 0, sizeof(result));

    int ret = t3verify_get_online_kami_count(verify, &result);

    jclass cls = (*env)->FindClass(env, "com/t3yanzheng/sdk/T3OnlineResult");
    if (cls == NULL) return NULL;
    jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, ctor);

    jfieldID f;
    f = (*env)->GetFieldID(env, cls, "success", "Z");
    if (f) (*env)->SetBooleanField(env, obj, f, ret == 0 ? JNI_TRUE : JNI_FALSE);
    f = (*env)->GetFieldID(env, cls, "error", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.error));
    f = (*env)->GetFieldID(env, cls, "count", "I");
    if (f) (*env)->SetIntField(env, obj, f, result.count);

    return obj;
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeGetOnlineUserCount
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeGetOnlineUserCount(
        JNIEnv *env, jobject thiz, jlong handle) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3OnlineResult result;
    memset(&result, 0, sizeof(result));

    int ret = t3verify_get_online_user_count(verify, &result);

    jclass cls = (*env)->FindClass(env, "com/t3yanzheng/sdk/T3OnlineResult");
    if (cls == NULL) return NULL;
    jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, ctor);

    jfieldID f;
    f = (*env)->GetFieldID(env, cls, "success", "Z");
    if (f) (*env)->SetBooleanField(env, obj, f, ret == 0 ? JNI_TRUE : JNI_FALSE);
    f = (*env)->GetFieldID(env, cls, "error", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.error));
    f = (*env)->GetFieldID(env, cls, "count", "I");
    if (f) (*env)->SetIntField(env, obj, f, result.count);

    return obj;
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeGetCloudDoc
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeGetCloudDoc(
        JNIEnv *env, jobject thiz, jlong handle, jstring token) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3CloudDocResult result;
    memset(&result, 0, sizeof(result));

    char *t = jstring_to_cstr(env, token);
    int ret = t3verify_get_cloud_doc(verify, t ? t : "", &result);
    free(t);

    jclass cls = (*env)->FindClass(env, "com/t3yanzheng/sdk/T3CloudDocResult");
    if (cls == NULL) return NULL;
    jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, ctor);

    jfieldID f;
    f = (*env)->GetFieldID(env, cls, "success", "Z");
    if (f) (*env)->SetBooleanField(env, obj, f, ret == 0 ? JNI_TRUE : JNI_FALSE);
    f = (*env)->GetFieldID(env, cls, "error", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.error));
    f = (*env)->GetFieldID(env, cls, "content", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.content));

    return obj;
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeAppSign
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeAppSign(
        JNIEnv *env, jobject thiz, jlong handle, jstring autograph) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3AppSignResult result;
    memset(&result, 0, sizeof(result));

    char *a = jstring_to_cstr(env, autograph);
    int ret = t3verify_app_sign(verify, a ? a : "", &result);
    free(a);

    jclass cls = (*env)->FindClass(env, "com/t3yanzheng/sdk/T3AppSignResult");
    if (cls == NULL) return NULL;
    jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, ctor);

    jfieldID f;
    f = (*env)->GetFieldID(env, cls, "success", "Z");
    if (f) (*env)->SetBooleanField(env, obj, f, ret == 0 ? JNI_TRUE : JNI_FALSE);
    f = (*env)->GetFieldID(env, cls, "error", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.error));
    f = (*env)->GetFieldID(env, cls, "msg", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.msg));
    f = (*env)->GetFieldID(env, cls, "autograph", "Ljava/lang/String;");
    if (f) (*env)->SetObjectField(env, obj, f, cstr_to_jstring(env, result.autograph));
    f = (*env)->GetFieldID(env, cls, "time", "J");
    if (f) (*env)->SetLongField(env, obj, f, result.time);

    return obj;
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeUnbindKami
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeUnbindKami(
        JNIEnv *env, jobject thiz, jlong handle, jstring kami, jstring imei) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *k = jstring_to_cstr(env, kami);
    char *i = jstring_to_cstr(env, imei);
    t3verify_unbind_kami(verify, k ? k : "", i ? i : "", &result);
    free(k); free(i);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeDisableKami
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeDisableKami(
        JNIEnv *env, jobject thiz, jlong handle, jstring kami) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *k = jstring_to_cstr(env, kami);
    t3verify_disable_kami(verify, k ? k : "", &result);
    free(k);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeQqLogin
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeQqLogin(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring openid, jstring accessToken) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3LoginResult result;
    memset(&result, 0, sizeof(result));

    char *o = jstring_to_cstr(env, openid);
    char *a = jstring_to_cstr(env, accessToken);

    int ret = t3verify_qq_login(verify, o ? o : "", a ? a : "", &result);
    free(o); free(a);

    return create_login_result(env, ret, &result);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeBindQq
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeBindQq(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring user, jstring pass, jstring openid, jstring accessToken) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *u = jstring_to_cstr(env, user);
    char *p = jstring_to_cstr(env, pass);
    char *o = jstring_to_cstr(env, openid);
    char *a = jstring_to_cstr(env, accessToken);

    t3verify_bind_qq(verify, u ? u : "", p ? p : "", o ? o : "", a ? a : "", &result);
    free(u); free(p); free(o); free(a);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeQqHeartbeat
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeQqHeartbeat(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring openid, jstring accessToken, jstring statecode) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *o = jstring_to_cstr(env, openid);
    char *a = jstring_to_cstr(env, accessToken);
    char *s = jstring_to_cstr(env, statecode);

    t3verify_qq_heartbeat(verify, o ? o : "", a ? a : "", s ? s : "", &result);
    free(o); free(a); free(s);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeUnbindQq
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeUnbindQq(
        JNIEnv *env, jobject thiz, jlong handle, jstring user, jstring pass) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *u = jstring_to_cstr(env, user);
    char *p = jstring_to_cstr(env, pass);

    t3verify_unbind_qq(verify, u ? u : "", p ? p : "", &result);
    free(u); free(p);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeGetVariableByKami
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeGetVariableByKami(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring kami, jstring valueid, jstring valuename) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3VariableResult result;
    memset(&result, 0, sizeof(result));

    char *k = jstring_to_cstr(env, kami);
    char *i = jstring_to_cstr(env, valueid);
    char *n = jstring_to_cstr(env, valuename);

    int ret = t3verify_get_variable_by_kami(verify, k ? k : "", i ? i : "", n ? n : "", &result);
    free(k); free(i); free(n);

    return create_variable_result(env, ret, &result);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeGetVariableByUser
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeGetVariableByUser(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring user, jstring pass, jstring valueid, jstring valuename) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3VariableResult result;
    memset(&result, 0, sizeof(result));

    char *u = jstring_to_cstr(env, user);
    char *p = jstring_to_cstr(env, pass);
    char *i = jstring_to_cstr(env, valueid);
    char *n = jstring_to_cstr(env, valuename);

    int ret = t3verify_get_variable_by_user(verify, u ? u : "", p ? p : "", i ? i : "", n ? n : "", &result);
    free(u); free(p); free(i); free(n);

    return create_variable_result(env, ret, &result);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeGetVariableByQq
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeGetVariableByQq(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring openid, jstring accessToken, jstring valueid, jstring valuename) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3VariableResult result;
    memset(&result, 0, sizeof(result));

    char *o = jstring_to_cstr(env, openid);
    char *a = jstring_to_cstr(env, accessToken);
    char *i = jstring_to_cstr(env, valueid);
    char *n = jstring_to_cstr(env, valuename);

    int ret = t3verify_get_variable_by_qq(verify, o ? o : "", a ? a : "", i ? i : "", n ? n : "", &result);
    free(o); free(a); free(i); free(n);

    return create_variable_result(env, ret, &result);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeModifyVariableByKami
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeModifyVariableByKami(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring kami, jstring valueid, jstring valuecontent) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *k = jstring_to_cstr(env, kami);
    char *i = jstring_to_cstr(env, valueid);
    char *c = jstring_to_cstr(env, valuecontent);

    t3verify_modify_variable_by_kami(verify, k ? k : "", i ? i : "", c ? c : "", &result);
    free(k); free(i); free(c);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeModifyVariableByUser
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeModifyVariableByUser(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring user, jstring pass, jstring valueid, jstring valuecontent) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *u = jstring_to_cstr(env, user);
    char *p = jstring_to_cstr(env, pass);
    char *i = jstring_to_cstr(env, valueid);
    char *c = jstring_to_cstr(env, valuecontent);

    t3verify_modify_variable_by_user(verify, u ? u : "", p ? p : "", i ? i : "", c ? c : "", &result);
    free(u); free(p); free(i); free(c);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeModifyCoreByKami
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeModifyCoreByKami(
        JNIEnv *env, jobject thiz, jlong handle, jstring kami, jstring core) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *k = jstring_to_cstr(env, kami);
    char *c = jstring_to_cstr(env, core);

    t3verify_modify_core_by_kami(verify, k ? k : "", c ? c : "", &result);
    free(k); free(c);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeModifyCoreByUser
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeModifyCoreByUser(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring user, jstring pass, jstring core) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *u = jstring_to_cstr(env, user);
    char *p = jstring_to_cstr(env, pass);
    char *c = jstring_to_cstr(env, core);

    t3verify_modify_core_by_user(verify, u ? u : "", p ? p : "", c ? c : "", &result);
    free(u); free(p); free(c);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeGetCoreByKami
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeGetCoreByKami(
        JNIEnv *env, jobject thiz, jlong handle, jstring kami) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3CoreResult result;
    memset(&result, 0, sizeof(result));

    char *k = jstring_to_cstr(env, kami);
    int ret = t3verify_get_core_by_kami(verify, k ? k : "", &result);
    free(k);

    return create_core_result(env, ret, &result);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeGetCoreByUser
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeGetCoreByUser(
        JNIEnv *env, jobject thiz, jlong handle, jstring user, jstring pass) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3CoreResult result;
    memset(&result, 0, sizeof(result));

    char *u = jstring_to_cstr(env, user);
    char *p = jstring_to_cstr(env, pass);
    int ret = t3verify_get_core_by_user(verify, u ? u : "", p ? p : "", &result);
    free(u); free(p);

    return create_core_result(env, ret, &result);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeGetUserCoreByQq
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeGetUserCoreByQq(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring openid, jstring accessToken) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3CoreResult result;
    memset(&result, 0, sizeof(result));

    char *o = jstring_to_cstr(env, openid);
    char *a = jstring_to_cstr(env, accessToken);
    int ret = t3verify_get_user_core_by_qq(verify, o ? o : "", a ? a : "", &result);
    free(o); free(a);

    return create_core_result(env, ret, &result);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeModifyUserCoreByQq
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeModifyUserCoreByQq(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring openid, jstring accessToken, jstring core) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *o = jstring_to_cstr(env, openid);
    char *a = jstring_to_cstr(env, accessToken);
    char *c = jstring_to_cstr(env, core);

    t3verify_modify_user_core_by_qq(verify, o ? o : "", a ? a : "", c ? c : "", &result);
    free(o); free(a); free(c);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeKamiRecharge
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeKamiRecharge(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring targetKami, jstring sourceKami) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *t = jstring_to_cstr(env, targetKami);
    char *s = jstring_to_cstr(env, sourceKami);

    t3verify_kami_recharge(verify, t ? t : "", s ? s : "", &result);
    free(t); free(s);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeChangePassword
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeChangePassword(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring user, jstring oldpass, jstring newpass) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *u = jstring_to_cstr(env, user);
    char *o = jstring_to_cstr(env, oldpass);
    char *n = jstring_to_cstr(env, newpass);

    t3verify_change_password(verify, u ? u : "", o ? o : "", n ? n : "", &result);
    free(u); free(o); free(n);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeUserCancel
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeUserCancel(
        JNIEnv *env, jobject thiz, jlong handle, jstring user, jstring pass) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *u = jstring_to_cstr(env, user);
    char *p = jstring_to_cstr(env, pass);

    t3verify_user_cancel(verify, u ? u : "", p ? p : "", &result);
    free(u); free(p);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeRecharge
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeRecharge(
        JNIEnv *env, jobject thiz, jlong handle, jstring user, jstring card) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *u = jstring_to_cstr(env, user);
    char *c = jstring_to_cstr(env, card);

    t3verify_recharge(verify, u ? u : "", c ? c : "", &result);
    free(u); free(c);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeUnbindUser
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeUnbindUser(
        JNIEnv *env, jobject thiz, jlong handle,
        jstring user, jstring pass, jstring imei) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *u = jstring_to_cstr(env, user);
    char *p = jstring_to_cstr(env, pass);
    char *i = jstring_to_cstr(env, imei);

    t3verify_unbind_user(verify, u ? u : "", p ? p : "", i ? i : "", &result);
    free(u); free(p); free(i);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeIpUnbindKami
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeIpUnbindKami(
        JNIEnv *env, jobject thiz, jlong handle, jstring kami) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *k = jstring_to_cstr(env, kami);
    t3verify_ip_unbind_kami(verify, k ? k : "", &result);
    free(k);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeIpUnbindUser
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeIpUnbindUser(
        JNIEnv *env, jobject thiz, jlong handle, jstring user, jstring pass) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *u = jstring_to_cstr(env, user);
    char *p = jstring_to_cstr(env, pass);

    t3verify_ip_unbind_user(verify, u ? u : "", p ? p : "", &result);
    free(u); free(p);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}

/*
 * Class:     com_t3yanzheng_sdk_T3Verify
 * Method:    nativeDisableUser
 */
JNIEXPORT jobject JNICALL Java_com_t3yanzheng_sdk_T3Verify_nativeDisableUser(
        JNIEnv *env, jobject thiz, jlong handle, jstring user, jstring pass) {

    T3Verify *verify = (T3Verify *)jlong_to_cptr(handle);
    T3Result result;
    memset(&result, 0, sizeof(result));

    char *u = jstring_to_cstr(env, user);
    char *p = jstring_to_cstr(env, pass);

    t3verify_disable_user(verify, u ? u : "", p ? p : "", &result);
    free(u); free(p);

    return create_result_object(env, "com/t3yanzheng/sdk/T3Result",
                                result.success, result.error, result.msg);
}
