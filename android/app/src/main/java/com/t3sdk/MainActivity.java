package com.t3sdk;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import org.json.JSONObject;

/**
 * T3 SDK Android 接入示例
 *
 * 1. 将 jniLibs 目录复制到你的 app/src/main/ 下
 * 2. 将 T3Verify.java 复制到你的项目中
 * 3. 在 AndroidManifest.xml 中添加网络权限:
 *    <uses-permission android:name="android.permission.INTERNET" />
 * 4. 在后台配置 API 协议为 HTTP（当前 socket 实现仅支持 HTTP）
 */
public class MainActivity extends Activity {

    private static final String TAG = "T3SDK";
    private T3Verify verify;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // 初始化 SDK
        verify = new T3Verify();
        verify.initRc4(
            "http://你的API域名",      // API 地址
            "你的APPKEY",              // APPKEY
            "你的RC4密钥"               // RC4 密钥
        );

        // 获取设备机器码
        String imei = verify.getMachineCode();
        Log.d(TAG, "机器码: " + imei);

        // 单码卡密登录（在子线程执行）
        new Thread(() -> {
            String result = verify.kamiLogin("你的卡密", imei);
            Log.d(TAG, "登录结果: " + result);
            try {
                JSONObject json = new JSONObject(result);
                if (json.getBoolean("success")) {
                    String statecode = json.getString("statecode");
                    String endTime = json.getString("end_time");
                    Log.d(TAG, "登录成功! 到期时间: " + endTime);

                    // 心跳验证（定时调用）
                    String hbResult = verify.kamiHeartbeat(statecode);
                    Log.d(TAG, "心跳: " + hbResult);
                } else {
                    String msg = json.getString("msg");
                    Log.e(TAG, "登录失败: " + msg);
                }
            } catch (Exception e) {
                Log.e(TAG, "解析错误", e);
            }
        }).start();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (verify != null) {
            verify.destroy();
        }
    }
}
