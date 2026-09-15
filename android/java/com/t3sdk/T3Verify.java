package com.t3sdk;

import org.json.JSONObject;
import org.json.JSONException;

/**
 * T3 网络验证 SDK - Android Java 封装
 *
 * 使用方式:
 *   T3Verify verify = new T3Verify();
 *   verify.init(apiUrl, appkey, ...);
 *   String result = verify.kamiLogin(kami, imei);
 *   JSONObject json = new JSONObject(result);
 *   if (json.getBoolean("success")) { ... }
 */
public class T3Verify {

    static {
        System.loadLibrary("t3sdk");
    }

    // ==================== 本地方法声明 ====================
    private native void nativeInit(String apiUrl, String appkey, int encType,
            int encodeType, boolean requestEncrypt, boolean responseEncrypt,
            String rc4Key, String desKey, String customB64Charset,
            boolean timestampCheck, int signType, int respFormat);

    private native void nativeDestroy();
    private native String nativeGetMachineCode();

    private native String nativeKamiLogin(String kami, String imei);
    private native String nativeKamiQuery(String kami);
    private native String nativeKamiHeartbeat(String statecode);
    private native String nativeKamiOnlineCount();
    private native String nativeGetKamiCore(String kami);

    private native String nativeUserLogin(String user, String pass, String imei);
    private native String nativeUserRegister(String user, String pass);
    private native String nativeUserHeartbeat(String statecode);
    private native String nativeUserRecharge(String user, String card);

    private native String nativeHeartbeatAny(String statecode);
    private native String nativeGetNotice();
    private native String nativeGetVersion();
    private native String nativeCheckUpdate(String ver);

    private native String nativeGetVariable(String valueid, String valuename);
    private native String nativeModifyVariable(String valueid, String valuecontent);

    private native String nativeAppSign(String autograph);

    private native String nativeQqLogin(String openid, String accessToken);
    private native String nativeQqHeartbeat(String openid, String accessToken, String statecode);

    private native String nativeUnbind(String kamiOrUser, String imei);
    private native String nativeIpUnbind(String kamiOrUser);
    private native String nativeDisable(String kamiOrUser);

    // ==================== 公共 API ====================

    /**
     * 初始化 SDK
     */
    public void init(String apiUrl, String appkey, int encType, int encodeType,
                     boolean requestEncrypt, boolean responseEncrypt,
                     String rc4Key, String desKey, String customB64Charset,
                     boolean timestampCheck, int signType, int respFormat) {
        nativeInit(apiUrl, appkey, encType, encodeType, requestEncrypt, responseEncrypt,
                rc4Key, desKey, customB64Charset, timestampCheck, signType, respFormat);
    }

    /**
     * 简化版初始化（适用于 RC4 + HEX + 时间戳 + 请求签名 + JSON 返回）
     */
    public void initRc4(String apiUrl, String appkey, String rc4Key) {
        nativeInit(apiUrl, appkey,
                1,   // encType: RC4
                2,   // encodeType: HEX
                true, true,
                rc4Key, null, null,
                true,   // timestampCheck
                1,      // signType: 请求签名
                1);     // respFormat: JSON
    }

    /** 销毁 SDK */
    public void destroy() {
        nativeDestroy();
    }

    /** 获取设备机器码 */
    public String getMachineCode() {
        return nativeGetMachineCode();
    }

    // ---- 单码卡密 ----
    public String kamiLogin(String kami, String imei) {
        return nativeKamiLogin(kami, imei);
    }

    public String kamiQuery(String kami) {
        return nativeKamiQuery(kami);
    }

    public String kamiHeartbeat(String statecode) {
        return nativeKamiHeartbeat(statecode);
    }

    public String kamiOnlineCount() {
        return nativeKamiOnlineCount();
    }

    public String getKamiCore(String kami) {
        return nativeGetKamiCore(kami);
    }

    // ---- 用户 ----
    public String userLogin(String user, String pass, String imei) {
        return nativeUserLogin(user, pass, imei);
    }

    public String userRegister(String user, String pass) {
        return nativeUserRegister(user, pass);
    }

    public String userHeartbeat(String statecode) {
        return nativeUserHeartbeat(statecode);
    }

    public String userRecharge(String user, String card) {
        return nativeUserRecharge(user, card);
    }

    // ---- 通用工具 ----
    public String heartbeatAny(String statecode) {
        return nativeHeartbeatAny(statecode);
    }

    public String getNotice() {
        return nativeGetNotice();
    }

    public String getVersion() {
        return nativeGetVersion();
    }

    public String checkUpdate(String ver) {
        return nativeCheckUpdate(ver);
    }

    // ---- 数据管理 ----
    public String getVariable(String valueid, String valuename) {
        return nativeGetVariable(valueid, valuename);
    }

    public String modifyVariable(String valueid, String valuecontent) {
        return nativeModifyVariable(valueid, valuecontent);
    }

    // ---- 安全功能 ----
    public String appSign(String autograph) {
        return nativeAppSign(autograph);
    }

    // ---- QQ 授权 ----
    public String qqLogin(String openid, String accessToken) {
        return nativeQqLogin(openid, accessToken);
    }

    public String qqHeartbeat(String openid, String accessToken, String statecode) {
        return nativeQqHeartbeat(openid, accessToken, statecode);
    }

    // ---- 通用接口 ----
    public String unbind(String kamiOrUser, String imei) {
        return nativeUnbind(kamiOrUser, imei);
    }

    public String ipUnbind(String kamiOrUser) {
        return nativeIpUnbind(kamiOrUser);
    }

    public String disable(String kamiOrUser) {
        return nativeDisable(kamiOrUser);
    }

    /**
     * 工具方法：解析返回的 JSON 字符串
     */
    public static JSONObject parseResult(String jsonStr) throws JSONException {
        return new JSONObject(jsonStr);
    }
}
