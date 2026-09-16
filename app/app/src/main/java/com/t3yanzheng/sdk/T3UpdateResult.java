package com.t3yanzheng.sdk;

public class T3UpdateResult {
    public boolean success;
    public String error;
    public boolean hasUpdate;
    public String ver;
    public String version;
    public String uplog;
    public String upurl;
    public String msg;

    public T3UpdateResult() {
        this.success = false;
        this.error = "";
        this.hasUpdate = false;
        this.ver = "";
        this.version = "";
        this.uplog = "";
        this.upurl = "";
        this.msg = "";
    }
}
