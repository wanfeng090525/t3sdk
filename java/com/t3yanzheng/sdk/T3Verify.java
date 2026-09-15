package com.t3yanzheng.sdk;

/**
 * T3 网络验证 SDK - Android Java 接口
 *
 * 依赖: libt3sdk.so (放在 jniLibs 目录)
 *
 * 使用示例:
 * <pre>
 *   T3Verify t3 = new T3Verify();
 *   t3.init(loginCode, noticeCode, versionCode, heartbeatCode, appkey, base64Charset);
 *   t3.setCode("query", queryCode);
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
     * 初始化 SDK (Base64 模式)
     *
     * @param loginCode     卡密登录调用码
     * @param noticeCode    获取公告调用码
     * @param versionCode   获取版本号调用码
     * @param heartbeatCode 心跳验证调用码
     * @param appkey        应用密钥
     * @param base64Charset 自定义 Base64 字符集 (64 字符)
     * @return true 成功, false 失败
     */
    public boolean init(String loginCode, String noticeCode, String versionCode,
                        String heartbeatCode, String appkey, String base64Charset) {
        return nativeInit(nativeHandle, loginCode, noticeCode, versionCode,
                heartbeatCode, appkey, base64Charset);
    }

    /**
     * 初始化 SDK (RSA 模式)
     */
    public boolean initRSA(String loginCode, String noticeCode, String versionCode,
                           String heartbeatCode, String appkey, String rsaPublicKey) {
        return nativeInitRSA(nativeHandle, loginCode, noticeCode, versionCode,
                heartbeatCode, appkey, rsaPublicKey);
    }

    /**
     * 设置其他接口的调用码
     *
     * @param field 字段名: query, register, user_login, user_heartbeat,
     *              qq_login, bind_qq, change_password, user_cancel, recharge,
     *              kami_recharge, unbind, ip_unbind, disable, check_update,
     *              get_variable, modify_variable, modify_core, get_kami_core,
     *              get_user_core, online_kami, online_user, cloud_doc, app_sign,
     *              qq_heartbeat, qq_get_variable, qq_get_core, qq_modify_core,
     *              qq_unbind, heartbeat_any
     * @param code  调用码
     */
    public void setCode(String field, String code) {
        nativeSetCode(nativeHandle, field, code);
    }

    /**
     * 获取机器码 (MAC 地址的 MD5)
     */
    public static native String nativeGetMachineCode();

    public static String getMachineCode() {
        return nativeGetMachineCode();
    }

    // ===== 卡密验证 =====

    public T3LoginResult login(String kami, String imei) {
        return nativeLogin(nativeHandle, kami, imei);
    }

    public T3QueryResult queryKami(String kami) {
        return nativeQueryKami(nativeHandle, kami);
    }

    public T3Result heartbeat(String kami, String statecode) {
        return nativeHeartbeat(nativeHandle, kami, statecode);
    }

    // ===== 数据与内容 =====

    public T3NoticeResult getNotice() {
        return nativeGetNotice(nativeHandle);
    }

    public T3VersionResult getLatestVersion() {
        return nativeGetLatestVersion(nativeHandle);
    }

    public T3UpdateResult checkUpdate(String ver) {
        return nativeCheckUpdate(nativeHandle, ver);
    }

    public T3CloudDocResult getCloudDoc(String token) {
        return nativeGetCloudDoc(nativeHandle, token);
    }

    public T3AppSignResult appSign(String autograph) {
        return nativeAppSign(nativeHandle, autograph);
    }

    // ===== 用户体系 =====

    public T3Result userRegister(String user, String pass, String email) {
        return nativeUserRegister(nativeHandle, user, pass, email);
    }

    public T3LoginResult userLogin(String user, String pass, String imei) {
        return nativeUserLogin(nativeHandle, user, pass, imei);
    }

    public T3Result userHeartbeat(String user, String pass, String statecode) {
        return nativeUserHeartbeat(nativeHandle, user, pass, statecode);
    }

    public T3Result heartbeatAny(String statecode) {
        return nativeHeartbeatAny(nativeHandle, statecode);
    }

    // ===== 设备与安全 =====

    public T3Result unbindKami(String kami, String imei) {
        return nativeUnbindKami(nativeHandle, kami, imei);
    }

    public T3Result disableKami(String kami) {
        return nativeDisableKami(nativeHandle, kami);
    }

    // ===== 在线数量 =====

    public T3OnlineResult getOnlineKamiCount() {
        return nativeGetOnlineKamiCount(nativeHandle);
    }

    public T3OnlineResult getOnlineUserCount() {
        return nativeGetOnlineUserCount(nativeHandle);
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

    // ===== Native 方法声明 =====

    private native long nativeCreate();
    private native void nativeDestroy(long handle);
    private native boolean nativeInit(long handle, String loginCode, String noticeCode,
                                      String versionCode, String heartbeatCode,
                                      String appkey, String base64Charset);
    private native boolean nativeInitRSA(long handle, String loginCode, String noticeCode,
                                         String versionCode, String heartbeatCode,
                                         String appkey, String rsaPublicKey);
    private native void nativeSetCode(long handle, String field, String code);

    private native T3LoginResult nativeLogin(long handle, String kami, String imei);
    private native T3Result nativeHeartbeat(long handle, String kami, String statecode);
    private native T3QueryResult nativeQueryKami(long handle, String kami);
    private native T3NoticeResult nativeGetNotice(long handle);
    private native T3VersionResult nativeGetLatestVersion(long handle);
    private native T3UpdateResult nativeCheckUpdate(long handle, String ver);
    private native T3CloudDocResult nativeGetCloudDoc(long handle, String token);
    private native T3AppSignResult nativeAppSign(long handle, String autograph);

    private native T3Result nativeUserRegister(long handle, String user, String pass, String email);
    private native T3LoginResult nativeUserLogin(long handle, String user, String pass, String imei);
    private native T3Result nativeUserHeartbeat(long handle, String user, String pass, String statecode);
    private native T3Result nativeHeartbeatAny(long handle, String statecode);

    private native T3Result nativeUnbindKami(long handle, String kami, String imei);
    private native T3Result nativeDisableKami(long handle, String kami);

    private native T3OnlineResult nativeGetOnlineKamiCount(long handle);
    private native T3OnlineResult nativeGetOnlineUserCount(long handle);
}
