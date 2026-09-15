package com.t3yanzheng.sdk;

/**
 * T3 验证 SDK - 通用结果
 */
public class T3Result {
    public boolean success;
    public String error;
    public String msg;

    public T3Result() {
        this.success = false;
        this.error = "";
        this.msg = "";
    }
}
