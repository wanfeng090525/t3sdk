package com.t3yanzheng.sdk;

public class T3AppSignResult {
    public boolean success;
    public String error;
    public String msg;
    public String autograph;
    public long time;

    public T3AppSignResult() {
        this.success = false;
        this.error = "";
        this.msg = "";
        this.autograph = "";
        this.time = 0;
    }
}
