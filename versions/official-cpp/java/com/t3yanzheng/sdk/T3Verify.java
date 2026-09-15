package com.t3yanzheng.sdk;

/**
 * T3 网络验证 SDK - Android Java 接口（精简版）
 *
 * 依赖: libt3sdk.so（官方 T3Example_AndroidJNI 源码编译，放在 jniLibs 目录）
 *
 * 说明:
 * - 所有 T3 后台凭证（调用码/APPKEY/公钥）已直接编译在 libt3sdk.so 中，Java 端无凭证
 * - 修改凭证: 编辑 android/jni/t3sdk_jni.cpp 顶部的 T3_* 宏后重新编译 .so
 * - 核心实现: 官方 t3sdk.cpp（仅优化了 Android 机器码获取）
 *
 * 使用示例:
 * <pre>
 *   T3Verify t3 = new T3Verify();
 *   t3.init();                              // 凭证已内置在 .so
 *   String imei = T3Verify.getMachineCode();
 *   T3LoginResult r = t3.login(kami, imei);
 *   if (r.success) {
 *       // 登录成功, r.statecode 用于心跳
 *       T3Result hb = t3.heartbeat(kami, r.statecode);
 *   }
 *   t3.destroy();
 * </pre>
 */
public class T3Verify {

    static {
        System.loadLibrary("t3sdk");
    }

    private long nativeHandle;

    public T3Verify() {
        nativeHandle = nativeCreate();
    }

    /**
     * 初始化 SDK（RSA 模式，凭证已编译在 .so 中）
     * @return true 成功, false 失败
     */
    public boolean init() {
        return nativeInit(nativeHandle);
    }

    /**
     * 获取机器码（官方实现 + Android 网卡遍历优化）
     */
    public static native String nativeGetMachineCode();

    public static String getMachineCode() {
        return nativeGetMachineCode();
    }

    /**
     * 卡密登录验证
     * @param kami 卡密
     * @param imei 机器码
     */
    public T3LoginResult login(String kami, String imei) {
        return nativeLogin(nativeHandle, kami, imei);
    }

    /**
     * 心跳验证（登录成功后周期性调用）
     * @param kami 卡密
     * @param statecode 登录返回的状态码
     */
    public T3Result heartbeat(String kami, String statecode) {
        return nativeHeartbeat(nativeHandle, kami, statecode);
    }

    /**
     * 释放资源
     */
    public void destroy() {
        if (nativeHandle != 0) {
            nativeDestroy(nativeHandle);
            nativeHandle = 0;
        }
    }

    @Override
    protected void finalize() throws Throwable {
        destroy();
        super.finalize();
    }

    // ===== Native 方法声明（与 libt3sdk.so 导出符号一一对应） =====

    private native long nativeCreate();
    private native void nativeDestroy(long handle);
    private native boolean nativeInit(long handle);
    private native T3LoginResult nativeLogin(long handle, String kami, String imei);
    private native T3Result nativeHeartbeat(long handle, String kami, String statecode);
}
