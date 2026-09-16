package com.t3yanzheng.sdk;

/**
 * T3 验证 SDK - 登录结果
 */
public class T3LoginResult {
    public boolean success;
    public String error;
    public String kami;      // 本次登录使用的卡密（native 自动登录时回填）
    public String id;
    public String endTime;
    public String statecode;
    public String recharge;
    public String useTime;
    public String amount;
    public String available;
    public String imei;
    public String change;
    public String core;

    public T3LoginResult() {
        this.success = false;
        this.error = "";
        this.kami = "";
        this.id = "";
        this.endTime = "";
        this.statecode = "";
        this.recharge = "";
        this.useTime = "";
        this.amount = "";
        this.available = "";
        this.imei = "";
        this.change = "";
        this.core = "";
    }
}
