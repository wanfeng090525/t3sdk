package com.t3yanzheng.sdk;

public class T3QueryResult {
    public boolean success;
    public String error;
    public String state;
    public String use;
    public String id;
    public String useTime;
    public String endTime;
    public String lineTime;
    public String line;
    public String amount;
    public String available;

    public T3QueryResult() {
        this.success = false;
        this.error = "";
        this.state = "";
        this.use = "";
        this.id = "";
        this.useTime = "";
        this.endTime = "";
        this.lineTime = "";
        this.line = "";
        this.amount = "";
        this.available = "";
    }
}
