package com.t3yanzheng.sdk;

/**
 * T3 验证 SDK - 核心数据结果
 */
public class T3CoreResult {
    public boolean success;
    public String error;
    public String core;

    public T3CoreResult() {
        this.success = false;
        this.error = "";
        this.core = "";
    }
}
