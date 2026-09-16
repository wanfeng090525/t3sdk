package com.example.myapp;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.Typeface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import com.t3yanzheng.sdk.T3NoticeResult;
import com.t3yanzheng.sdk.T3Result;
import com.t3yanzheng.sdk.T3UpdateResult;
import com.t3yanzheng.sdk.T3Verify;
import com.t3yanzheng.sdk.T3VersionResult;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * 主页面 - iOS 原生风格（分组表格样式）
 *
 * 功能：账号信息、公告、检查更新、解绑卡密、退出登录、自动心跳
 */
public class MainActivity extends Activity {

    private static final long HEARTBEAT_INTERVAL = 30_000L;

    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private final Handler mainHandler = new Handler(Looper.getMainLooper());

    private T3Verify t3;
    private String kami = "";
    private String imei = "";
    private String statecode = "";
    private String endTime = "";
    private String appVer = "1.0";

    private TextView tvHeartbeatStatus;
    private boolean destroyed = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        SharedPreferences sp = getSharedPreferences(LoginActivity.PREFS_NAME, MODE_PRIVATE);
        kami = sp.getString(LoginActivity.KEY_KAMI, "");
        imei = sp.getString(LoginActivity.KEY_IMEI, "");
        statecode = sp.getString(LoginActivity.KEY_STATECODE, "");
        endTime = sp.getString(LoginActivity.KEY_END_TIME, "");

        if (TextUtils.isEmpty(kami)) {
            backToLogin();
            return;
        }

        // 初始化 SDK（凭证在 .so 中）
        t3 = new T3Verify();
        t3.init();

        setContentView(buildContent());
        startHeartbeat();
    }

    // ========== UI 构建（iOS 分组表格风格） ==========

    private View buildContent() {
        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(0xFFF2F2F7);
        scroll.setFillViewport(true);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        scroll.addView(root, new ScrollView.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT));

        // 导航栏
        root.addView(navBar());

        // 分组一：账号信息
        root.addView(sectionHeader("账号信息"));
        LinearLayout accountGroup = group();
        addInfoCell(accountGroup, "卡密", kami);
        addInfoCell(accountGroup, "机器码", imei);
        addInfoCell(accountGroup, "到期时间", TextUtils.isEmpty(endTime) ? "—" : endTime);
        addHeartbeatCell(accountGroup);
        root.addView(groupContainer(accountGroup));

        // 分组二：功能
        root.addView(sectionHeader("功能"));
        LinearLayout funcGroup = group();
        addActionCell(funcGroup, "公告", true, v -> loadNotice());
        addActionCell(funcGroup, "检查更新", true, v -> checkUpdate());
        addActionCell(funcGroup, "解绑卡密", false, v -> confirmUnbind());
        root.addView(groupContainer(funcGroup));

        // 分组三：其他
        root.addView(sectionHeader("其他"));
        LinearLayout otherGroup = group();
        addInfoCell(otherGroup, "版本", appVer);
        root.addView(groupContainer(otherGroup));

        // 退出登录
        TextView logout = new TextView(this);
        logout.setText("退出登录");
        logout.setTextSize(17);
        logout.setGravity(Gravity.CENTER);
        logout.setTextColor(0xFFFF3B30);
        logout.setBackground(bg(R.drawable.bg_btn_red));
        logout.setPadding(0, 0, 0, 0);
        logout.setOnClickListener(v -> confirmLogout());
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, dp(50));
        lp.setMargins(dp(16), dp(24), dp(16), dp(24));
        root.addView(logout, lp);

        return scroll;
    }

    private View navBar() {
        LinearLayout bar = new LinearLayout(this);
        bar.setOrientation(LinearLayout.HORIZONTAL);
        bar.setGravity(Gravity.CENTER_VERTICAL);
        bar.setBackgroundColor(Color.WHITE);
        bar.setPadding(dp(16), dp(8), dp(16), dp(8));

        TextView left = new TextView(this);
        left.setText("");

        TextView title = new TextView(this);
        title.setText("我的");
        title.setTextSize(17);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setTextColor(0xFF1C1C1E);
        title.setGravity(Gravity.CENTER);

        TextView right = new TextView(this);
        right.setText("");
        right.setTextSize(16);
        right.setTextColor(0xFF007AFF);

        bar.addView(left, new LinearLayout.LayoutParams(dp(60),
                LinearLayout.LayoutParams.WRAP_CONTENT));
        bar.addView(title, new LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        bar.addView(right, new LinearLayout.LayoutParams(dp(60),
                LinearLayout.LayoutParams.WRAP_CONTENT));

        // 底部分割线
        LinearLayout wrap = new LinearLayout(this);
        wrap.setOrientation(LinearLayout.VERTICAL);
        wrap.setBackgroundColor(Color.WHITE);
        wrap.addView(bar, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, dp(52)));
        View divider = new View(this);
        divider.setBackgroundColor(0xFFE5E5EA);
        wrap.addView(divider, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 1));
        return wrap;
    }

    private TextView sectionHeader(String text) {
        TextView tv = new TextView(this);
        tv.setText(text);
        tv.setTextSize(13);
        tv.setTextColor(0xFF8E8E93);
        tv.setPadding(dp(20), dp(20), dp(20), dp(8));
        return tv;
    }

    private LinearLayout group() {
        LinearLayout g = new LinearLayout(this);
        g.setOrientation(LinearLayout.VERTICAL);
        return g;
    }

    private LinearLayout groupContainer(LinearLayout group) {
        LinearLayout wrap = new LinearLayout(this);
        wrap.setOrientation(LinearLayout.VERTICAL);
        wrap.setBackground(bg(R.drawable.bg_cell));
        wrap.setPadding(dp(16), 0, dp(16), 0);
        wrap.addView(group, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT);
        lp.setMargins(dp(16), 0, dp(16), 0);
        wrap.setLayoutParams(lp);
        return wrap;
    }

    private void addInfoCell(LinearLayout group, String title, String value) {
        LinearLayout cell = new LinearLayout(this);
        cell.setOrientation(LinearLayout.HORIZONTAL);
        cell.setGravity(Gravity.CENTER_VERTICAL);
        cell.setPadding(0, dp(14), 0, dp(14));
        cell.setBackground(bg(R.drawable.bg_cell_plain));

        TextView t = new TextView(this);
        t.setText(title);
        t.setTextSize(16);
        t.setTextColor(0xFF1C1C1E);

        TextView v = new TextView(this);
        v.setText(TextUtils.isEmpty(value) ? "—" : value);
        v.setTextSize(16);
        v.setTextColor(0xFF8E8E93);
        v.setMaxLines(1);
        v.setEllipsize(TextUtils.TruncateAt.MIDDLE);
        v.setGravity(Gravity.END);

        cell.addView(t, new LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        cell.addView(v, new LinearLayout.LayoutParams(dp(220),
                LinearLayout.LayoutParams.WRAP_CONTENT));
        group.addView(cell);
        group.addView(divider());
    }

    private void addHeartbeatCell(LinearLayout group) {
        LinearLayout cell = new LinearLayout(this);
        cell.setOrientation(LinearLayout.HORIZONTAL);
        cell.setGravity(Gravity.CENTER_VERTICAL);
        cell.setPadding(0, dp(14), 0, dp(14));
        cell.setBackground(bg(R.drawable.bg_cell_plain));

        TextView t = new TextView(this);
        t.setText("连接状态");
        t.setTextSize(16);
        t.setTextColor(0xFF1C1C1E);

        tvHeartbeatStatus = new TextView(this);
        tvHeartbeatStatus.setText("心跳中…");
        tvHeartbeatStatus.setTextSize(16);
        tvHeartbeatStatus.setTextColor(0xFF8E8E93);
        tvHeartbeatStatus.setGravity(Gravity.END);

        cell.addView(t, new LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        cell.addView(tvHeartbeatStatus, new LinearLayout.LayoutParams(dp(220),
                LinearLayout.LayoutParams.WRAP_CONTENT));
        group.addView(cell);
        group.addView(divider());
    }

    private void addActionCell(LinearLayout group, String title, boolean arrow,
                               View.OnClickListener listener) {
        LinearLayout cell = new LinearLayout(this);
        cell.setOrientation(LinearLayout.HORIZONTAL);
        cell.setGravity(Gravity.CENTER_VERTICAL);
        cell.setPadding(0, dp(14), 0, dp(14));
        cell.setBackground(bg(R.drawable.bg_cell_plain));
        cell.setOnClickListener(listener);

        TextView t = new TextView(this);
        t.setText(title);
        t.setTextSize(16);
        t.setTextColor(0xFF1C1C1E);

        cell.addView(t, new LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        if (arrow) {
            TextView chevron = new TextView(this);
            chevron.setText("›");
            chevron.setTextSize(20);
            chevron.setTextColor(0xFFC7C7CC);
            chevron.setGravity(Gravity.CENTER);
            cell.addView(chevron, new LinearLayout.LayoutParams(dp(24),
                    LinearLayout.LayoutParams.WRAP_CONTENT));
        }
        group.addView(cell);
        group.addView(divider());
    }

    private View divider() {
        View v = new View(this);
        v.setBackgroundColor(0xFFE5E5EA);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 1);
        lp.setMargins(0, 0, 0, 0);
        v.setLayoutParams(lp);
        return v;
    }

    private android.graphics.drawable.Drawable bg(int res) {
        return getDrawable(res);
    }

    private int dp(int v) {
        return Math.round(v * density());
    }

    private float density() {
        return getResources().getDisplayMetrics().density;
    }

    // ========== 业务功能 ==========

    private void loadNotice() {
        executor.execute(() -> {
            T3NoticeResult r = t3.getNotice();
            mainHandler.post(() -> {
                if (destroyed) return;
                if (r != null && r.success) {
                    showDialog("公告", r.notice);
                } else {
                    toast(r != null && !TextUtils.isEmpty(r.error) ? r.error : "公告获取失败");
                }
            });
        });
    }

    private void checkUpdate() {
        executor.execute(() -> {
            T3VersionResult ver = t3.getLatestVersion();
            if (ver == null || !ver.success) {
                mainHandler.post(() -> {
                    if (!destroyed) toast(ver != null && !TextUtils.isEmpty(ver.error) ? ver.error : "版本查询失败");
                });
                return;
            }
            T3UpdateResult up = t3.checkUpdate(ver.version);
            mainHandler.post(() -> {
                if (destroyed) return;
                if (up != null && up.success && up.hasUpdate) {
                    showUpdateDialog(up);
                } else {
                    toast("已是最新版本");
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
        AlertDialog.Builder b = new AlertDialog.Builder(this);
        b.setTitle("解绑卡密");
        b.setMessage("确定要解绑当前卡密 " + kami + " 吗？解绑后可在其他设备上重新绑定。");
        b.setNegativeButton("取消", null);
        b.setPositiveButton("解绑", (d, w) -> {
            executor.execute(() -> {
                T3Result r = t3.unbindKami(kami, imei);
                mainHandler.post(() -> {
                    if (destroyed) return;
                    toast(r != null && r.success ? "解绑成功" :
                            (r != null && !TextUtils.isEmpty(r.error) ? r.error : "解绑失败"));
                });
            });
        });
        b.show();
    }

    private void confirmLogout() {
        AlertDialog.Builder b = new AlertDialog.Builder(this);
        b.setTitle("退出登录");
        b.setMessage("确定要退出当前账号吗？");
        b.setNegativeButton("取消", null);
        b.setPositiveButton("退出", (d, w) -> {
            // 退出登录同时清除 .so 内保存的卡密，下次启动不再自动登录
            if (t3 != null) t3.clearSavedCard();
            SharedPreferences sp = getSharedPreferences(LoginActivity.PREFS_NAME, MODE_PRIVATE);
            sp.edit().clear().apply();
            backToLogin();
        });
        b.show();
    }

    // ========== 心跳 ==========

    private void startHeartbeat() {
        if (TextUtils.isEmpty(statecode)) return;
        mainHandler.postDelayed(heartbeatTask, HEARTBEAT_INTERVAL);
    }

    private final Runnable heartbeatTask = new Runnable() {
        @Override
        public void run() {
            if (destroyed) return;
            executor.execute(() -> {
                final T3Result r = t3.heartbeat(kami, statecode);
                mainHandler.post(() -> {
                    if (destroyed) return;
                    boolean ok = r != null && r.success;
                    tvHeartbeatStatus.setText(ok ? "正常" : "异常");
                    tvHeartbeatStatus.setTextColor(ok ? 0xFF34C759 : 0xFFFF3B30);
                });
            });
            mainHandler.postDelayed(this, HEARTBEAT_INTERVAL);
        }
    };

    // ========== 工具 ==========

    private void showDialog(String title, String msg) {
        AlertDialog.Builder b = new AlertDialog.Builder(this);
        b.setTitle(title);
        b.setMessage(TextUtils.isEmpty(msg) ? "(无内容)" : msg);
        b.setPositiveButton("确定", (d, w) -> d.dismiss());
        b.show();
    }

    private void toast(String msg) {
        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show();
    }

    private void backToLogin() {
        Intent i = new Intent(this, LoginActivity.class);
        i.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(i);
        finish();
    }

    @Override
    protected void onDestroy() {
        destroyed = true;
        mainHandler.removeCallbacks(heartbeatTask);
        if (t3 != null) {
            t3.destroy();
            t3 = null;
        }
        executor.shutdown();
        super.onDestroy();
    }
}
