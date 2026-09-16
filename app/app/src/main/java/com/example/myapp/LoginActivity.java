package com.example.myapp;

import android.app.Activity;
import android.app.AlertDialog;
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
import com.t3yanzheng.sdk.T3NoticeResult;
import com.t3yanzheng.sdk.T3Result;
import com.t3yanzheng.sdk.T3UpdateResult;
import com.t3yanzheng.sdk.T3Verify;
import com.t3yanzheng.sdk.T3VersionResult;

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

        // 设置卡密存储目录，自动登录在 .so 内完成（读取/验证清卡都在 native 侧）
        if (ok) {
            t3.setStoragePath(getFilesDir().getAbsolutePath());
        }

        // 获取机器码
        executor.execute(() -> {
            machineCode = T3Verify.getMachineCode();
            mainHandler.post(() -> {
                tvMachineCode.setText(TextUtils.isEmpty(machineCode) ? "获取失败" : machineCode);
                if (!ok) tvError.setText("SDK 初始化失败，请检查 libt3sdk.so");
                // 自动登录（官方 Fullscreen 示例逻辑，.so 内实现）
                if (ok && t3.hasSavedCard()) {
                    tryNativeAutoLogin();
                }
            });
        });

        tvCopy.setOnClickListener(v -> {
            if (TextUtils.isEmpty(machineCode)) return;
            ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
            cm.setPrimaryClip(ClipData.newPlainText("machine", machineCode));
            Toast.makeText(this, "已复制机器码", Toast.LENGTH_SHORT).show();
        });

        btnLogin.setOnClickListener(v -> doLogin());

        // 更多功能：公告 / 检查更新 / 解绑卡密
        findViewById(R.id.rowNotice).setOnClickListener(v -> loadNotice());
        findViewById(R.id.rowUpdate).setOnClickListener(v -> checkUpdate());
        findViewById(R.id.rowUnbind).setOnClickListener(v -> confirmUnbind());
    }

    // ========== 自动登录（官方 Fullscreen 示例逻辑，在 .so 内实现） ==========

    private void tryNativeAutoLogin() {
        final AlertDialog loading = new AlertDialog.Builder(this)
                .setMessage("正在自动登录...")
                .setCancelable(false)
                .create();
        loading.show();
        btnLogin.setEnabled(false);

        executor.execute(() -> {
            // 读取/验证/失败清卡全部由 .so 完成，Java 只拿结果
            final T3LoginResult r = t3.autoLogin();
            mainHandler.post(() -> {
                loading.dismiss();
                if (r != null && r.success) {
                    // 自动登录成功，直接进入主界面（r.kami 由 .so 回填）
                    saveLogin(r.kami, r);
                    Intent intent = new Intent(LoginActivity.this, MainActivity.class);
                    intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
                    startActivity(intent);
                    finish();
                } else {
                    // 自动登录失败（.so 已清除保存的卡密），转手动输入
                    clearSavedState();
                    showError(r != null && !TextUtils.isEmpty(r.error) ? r.error : "自动登录失败");
                    btnLogin.setEnabled(true);
                }
            });
        });
    }

    private void clearSavedState() {
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE).edit()
                .remove(KEY_STATECODE)
                .remove(KEY_END_TIME)
                .apply();
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

    // ========== 更多功能：公告 / 检查更新 / 解绑卡密 ==========

    private void loadNotice() {
        executor.execute(() -> {
            T3NoticeResult r = t3.getNotice();
            mainHandler.post(() -> {
                if (r != null && r.success) {
                    showDialog("公告", r.notice);
                } else {
                    showError(r != null && !TextUtils.isEmpty(r.error) ? r.error : "公告获取失败");
                }
            });
        });
    }

    private void checkUpdate() {
        executor.execute(() -> {
            T3VersionResult ver = t3.getLatestVersion();
            if (ver == null || !ver.success) {
                mainHandler.post(() -> showError(ver != null && !TextUtils.isEmpty(ver.error) ? ver.error : "版本查询失败"));
                return;
            }
            T3UpdateResult up = t3.checkUpdate(ver.version);
            mainHandler.post(() -> {
                if (up != null && up.success && up.hasUpdate) {
                    showUpdateDialog(up);
                } else {
                    showError("已是最新版本");
                }
            });
        });
    }

    private void showUpdateDialog(T3UpdateResult up) {
        AlertDialog.Builder b = new AlertDialog.Builder(this);
        b.setTitle("发现新版本 " + up.ver);
        b.setMessage(TextUtils.isEmpty(up.uplog) ? "请前往下载更新" : up.uplog);
        b.setPositiveButton("确定", (d, w) -> d.dismiss());
        b.show();
    }

    private void confirmUnbind() {
        final String kami = etKami.getText().toString().trim();
        if (TextUtils.isEmpty(kami)) {
            showError("请输入要解绑的卡密");
            return;
        }
        if (TextUtils.isEmpty(machineCode)) {
            showError("机器码获取失败，无法解绑");
            return;
        }
        AlertDialog.Builder b = new AlertDialog.Builder(this);
        b.setTitle("解绑卡密");
        b.setMessage("确定要解绑卡密 " + kami + " 吗？解绑后可在其他设备重新绑定。");
        b.setNegativeButton("取消", null);
        b.setPositiveButton("解绑", (d, w) -> {
            executor.execute(() -> {
                T3Result r = t3.unbindKami(kami, machineCode);
                mainHandler.post(() -> {
                    if (r != null && r.success) {
                        // 解绑成功后清除 .so 内保存的卡密，不再自动登录
                        t3.clearSavedCard();
                        showError("");
                        Toast.makeText(this, "解绑成功", Toast.LENGTH_SHORT).show();
                    } else {
                        showError(r != null && !TextUtils.isEmpty(r.error) ? r.error : "解绑失败");
                    }
                });
            });
        });
        b.show();
    }

    private void showDialog(String title, String msg) {
        AlertDialog.Builder b = new AlertDialog.Builder(this);
        b.setTitle(title);
        b.setMessage(TextUtils.isEmpty(msg) ? "(无内容)" : msg);
        b.setPositiveButton("确定", (d, w) -> d.dismiss());
        b.show();
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
