package com.t3yanzheng.sdk;

/**
 * T3 验证 SDK - 远程变量结果
 */
public class T3VariableResult {
    public boolean success;
    public String error;
    public String value;

    public T3VariableResult() {
        this.success = false;
        this.error = "";
        this.value = "";
    }
}
