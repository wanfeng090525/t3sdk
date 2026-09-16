package com.example.myapp;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import com.t3yanzheng.sdk.T3LoginResult;
import com.t3yanzheng.sdk.T3Verify;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * 登录页 - iOS 原生风格
 *
 * 凭证已内置在 libt3sdk.so 中，这里只负责界面与调用。
 */
public class LoginActivity extends Activity {

    public static final String PREFS_NAME = "t3_prefs";
    public static final String KEY_KAMI = "kami";
    public static final String KEY_IMEI = "imei";
    public static final String KEY_STATECODE = "statecode";
    public static final String KEY_END_TIME = "end_time";

    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private final Handler mainHandler = new Handler(Looper.getMainLooper());

    private T3Verify t3;
    private EditText etKami;
    private TextView tvMachineCode;
    private TextView tvError;
    private Button btnLogin;
    private String machineCode = "";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_login);

        etKami = findViewById(R.id.etKami);
        tvMachineCode = findViewById(R.id.tvMachineCode);
        tvError = findViewById(R.id.tvError);
        btnLogin = findViewById(R.id.btnLogin);
        TextView tvCopy = findViewById(R.id.tvCopyMachine);

        // 初始化 SDK（凭证在 .so 中）
        t3 = new T3Verify();
        final boolean ok = t3.init();

        // 获取机器码
        executor.execute(() -> {
            machineCode = T3Verify.getMachineCode();
            mainHandler.post(() -> {
                tvMachineCode.setText(TextUtils.isEmpty(machineCode) ? "获取失败" : machineCode);
                if (!ok) tvError.setText("SDK 初始化失败，请检查 libt3sdk.so");
            });
        });

        tvCopy.setOnClickListener(v -> {
            if (TextUtils.isEmpty(machineCode)) return;
            ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
            cm.setPrimaryClip(ClipData.newPlainText("machine", machineCode));
            Toast.makeText(this, "已复制机器码", Toast.LENGTH_SHORT).show();
        });

        btnLogin.setOnClickListener(v -> doLogin());
    }

    private void doLogin() {
        final String kami = etKami.getText().toString().trim();
        if (TextUtils.isEmpty(kami)) {
            showError("请输入卡密");
            return;
        }
        if (TextUtils.isEmpty(machineCode)) {
            showError("机器码获取失败，请重试");
            return;
        }

        btnLogin.setEnabled(false);
        showError("");
        executor.execute(() -> {
            T3LoginResult r = t3.login(kami, machineCode);
            mainHandler.post(() -> {
                btnLogin.setEnabled(true);
                if (r != null && r.success) {
                    saveLogin(kami, r);
                    Intent intent = new Intent(LoginActivity.this, MainActivity.class);
                    intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
                    startActivity(intent);
                    finish();
                } else {
                    showError(r != null && !TextUtils.isEmpty(r.error) ? r.error : "登录失败");
                }
            });
        });
    }

    private void saveLogin(String kami, T3LoginResult r) {
        SharedPreferences sp = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        sp.edit()
                .putString(KEY_KAMI, kami)
                .putString(KEY_IMEI, machineCode)
                .putString(KEY_STATECODE, r.statecode)
                .putString(KEY_END_TIME, r.endTime)
                .apply();
    }

    private void showError(String msg) {
        tvError.setText(msg);
        tvError.setVisibility(TextUtils.isEmpty(msg) ? View.GONE : View.VISIBLE);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (t3 != null) {
            t3.destroy();
            t3 = null;
        }
        executor.shutdown();
    }
}
