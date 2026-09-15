package com.t3yanzheng.sdk;

/**
 * T3 网络验证 SDK - Android Java 接口（官方完整能力版，C语言实现）
 *
 * 依赖: libt3sdk.so (放在 jniLibs 目录)
 *
 * 注意: T3 后台凭证（调用码、APPKEY 等）已直接编译在 libt3sdk.so 中，
 *       Java 端无需传递。如需修改凭证，编辑 t3sdk_jni.c 顶部的 T3_* 宏后重新编译 .so。
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
     * 初始化 SDK (Base64 模式)
     * 凭证已编译在 .so 中，无需传参
     * @return true 成功, false 失败
     */
    public boolean init() {
        return nativeInit(nativeHandle);
    }

    /**
     * 初始化 SDK (RSA 模式)
     * 凭证和公钥已编译在 .so 中，无需传参
     */
    public boolean initRSA() {
        return nativeInitRSA(nativeHandle);
    }

    /**
     * 设置其他接口的调用码
     */
    public void setCode(String field, String code) {
        nativeSetCode(nativeHandle, field, code);
    }

    /**
     * 获取机器码 (MAC 地址的 MD5 + Android 网卡遍历优化)
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

    /** 卡密充值（用 sourceKami 给 targetKami 充值） */
    public T3Result kamiRecharge(String targetKami, String sourceKami) {
        return nativeKamiRecharge(nativeHandle, targetKami, sourceKami);
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

    public T3Result changePassword(String user, String oldpass, String newpass) {
        return nativeChangePassword(nativeHandle, user, oldpass, newpass);
    }

    public T3Result userCancel(String user, String pass) {
        return nativeUserCancel(nativeHandle, user, pass);
    }

    public T3Result recharge(String user, String card) {
        return nativeRecharge(nativeHandle, user, card);
    }

    public T3Result heartbeatAny(String statecode) {
        return nativeHeartbeatAny(nativeHandle, statecode);
    }

    // ===== QQ 体系 =====

    public T3LoginResult qqLogin(String openid, String accessToken) {
        return nativeQqLogin(nativeHandle, openid, accessToken);
    }

    public T3Result bindQq(String user, String pass, String openid, String accessToken) {
        return nativeBindQq(nativeHandle, user, pass, openid, accessToken);
    }

    public T3Result unbindQq(String user, String pass) {
        return nativeUnbindQq(nativeHandle, user, pass);
    }

    public T3Result qqHeartbeat(String openid, String accessToken, String statecode) {
        return nativeQqHeartbeat(nativeHandle, openid, accessToken, statecode);
    }

    // ===== 设备与安全 =====

    public T3Result unbindKami(String kami, String imei) {
        return nativeUnbindKami(nativeHandle, kami, imei);
    }

    public T3Result unbindUser(String user, String pass, String imei) {
        return nativeUnbindUser(nativeHandle, user, pass, imei);
    }

    public T3Result ipUnbindKami(String kami) {
        return nativeIpUnbindKami(nativeHandle, kami);
    }

    public T3Result ipUnbindUser(String user, String pass) {
        return nativeIpUnbindUser(nativeHandle, user, pass);
    }

    public T3Result disableKami(String kami) {
        return nativeDisableKami(nativeHandle, kami);
    }

    public T3Result disableUser(String user, String pass) {
        return nativeDisableUser(nativeHandle, user, pass);
    }

    // ===== 远程变量 =====

    public T3VariableResult getVariableByKami(String kami, String valueid, String valuename) {
        return nativeGetVariableByKami(nativeHandle, kami, valueid, valuename);
    }

    public T3VariableResult getVariableByUser(String user, String pass, String valueid, String valuename) {
        return nativeGetVariableByUser(nativeHandle, user, pass, valueid, valuename);
    }

    public T3VariableResult getVariableByQq(String openid, String accessToken, String valueid, String valuename) {
        return nativeGetVariableByQq(nativeHandle, openid, accessToken, valueid, valuename);
    }

    public T3Result modifyVariableByKami(String kami, String valueid, String valuecontent) {
        return nativeModifyVariableByKami(nativeHandle, kami, valueid, valuecontent);
    }

    public T3Result modifyVariableByUser(String user, String pass, String valueid, String valuecontent) {
        return nativeModifyVariableByUser(nativeHandle, user, pass, valueid, valuecontent);
    }

    // ===== 核心数据 =====

    public T3CoreResult getCoreByKami(String kami) {
        return nativeGetCoreByKami(nativeHandle, kami);
    }

    public T3CoreResult getCoreByUser(String user, String pass) {
        return nativeGetCoreByUser(nativeHandle, user, pass);
    }

    public T3CoreResult getUserCoreByQq(String openid, String accessToken) {
        return nativeGetUserCoreByQq(nativeHandle, openid, accessToken);
    }

    public T3Result modifyCoreByKami(String kami, String core) {
        return nativeModifyCoreByKami(nativeHandle, kami, core);
    }

    public T3Result modifyCoreByUser(String user, String pass, String core) {
        return nativeModifyCoreByUser(nativeHandle, user, pass, core);
    }

    public T3Result modifyUserCoreByQq(String openid, String accessToken, String core) {
        return nativeModifyUserCoreByQq(nativeHandle, openid, accessToken, core);
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

    // ===== Native 方法声明（与 libt3sdk.so 导出符号一一对应） =====

    private native long nativeCreate();
    private native void nativeDestroy(long handle);
    private native boolean nativeInit(long handle);
    private native boolean nativeInitRSA(long handle);
    private native void nativeSetCode(long handle, String field, String code);

    private native T3LoginResult nativeLogin(long handle, String kami, String imei);
    private native T3Result nativeHeartbeat(long handle, String kami, String statecode);
    private native T3QueryResult nativeQueryKami(long handle, String kami);
    private native T3Result nativeKamiRecharge(long handle, String targetKami, String sourceKami);
    private native T3NoticeResult nativeGetNotice(long handle);
    private native T3VersionResult nativeGetLatestVersion(long handle);
    private native T3UpdateResult nativeCheckUpdate(long handle, String ver);
    private native T3CloudDocResult nativeGetCloudDoc(long handle, String token);
    private native T3AppSignResult nativeAppSign(long handle, String autograph);

    private native T3Result nativeUserRegister(long handle, String user, String pass, String email);
    private native T3LoginResult nativeUserLogin(long handle, String user, String pass, String imei);
    private native T3Result nativeUserHeartbeat(long handle, String user, String pass, String statecode);
    private native T3Result nativeChangePassword(long handle, String user, String oldpass, String newpass);
    private native T3Result nativeUserCancel(long handle, String user, String pass);
    private native T3Result nativeRecharge(long handle, String user, String card);
    private native T3Result nativeHeartbeatAny(long handle, String statecode);

    private native T3LoginResult nativeQqLogin(long handle, String openid, String accessToken);
    private native T3Result nativeBindQq(long handle, String user, String pass, String openid, String accessToken);
    private native T3Result nativeUnbindQq(long handle, String user, String pass);
    private native T3Result nativeQqHeartbeat(long handle, String openid, String accessToken, String statecode);

    private native T3Result nativeUnbindKami(long handle, String kami, String imei);
    private native T3Result nativeUnbindUser(long handle, String user, String pass, String imei);
    private native T3Result nativeIpUnbindKami(long handle, String kami);
    private native T3Result nativeIpUnbindUser(long handle, String user, String pass);
    private native T3Result nativeDisableKami(long handle, String kami);
    private native T3Result nativeDisableUser(long handle, String user, String pass);

    private native T3VariableResult nativeGetVariableByKami(long handle, String kami, String valueid, String valuename);
    private native T3VariableResult nativeGetVariableByUser(long handle, String user, String pass, String valueid, String valuename);
    private native T3VariableResult nativeGetVariableByQq(long handle, String openid, String accessToken, String valueid, String valuename);
    private native T3Result nativeModifyVariableByKami(long handle, String kami, String valueid, String valuecontent);
    private native T3Result nativeModifyVariableByUser(long handle, String user, String pass, String valueid, String valuecontent);

    private native T3CoreResult nativeGetCoreByKami(long handle, String kami);
    private native T3CoreResult nativeGetCoreByUser(long handle, String user, String pass);
    private native T3CoreResult nativeGetUserCoreByQq(long handle, String openid, String accessToken);
    private native T3Result nativeModifyCoreByKami(long handle, String kami, String core);
    private native T3Result nativeModifyCoreByUser(long handle, String user, String pass, String core);
    private native T3Result nativeModifyUserCoreByQq(long handle, String openid, String accessToken, String core);

    private native T3OnlineResult nativeGetOnlineKamiCount(long handle);
    private native T3OnlineResult nativeGetOnlineUserCount(long handle);
}
