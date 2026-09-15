package com.example.myapp;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import com.t3yanzheng.sdk.T3Helper;

/**
 * 登录界面
 * 用户输入卡密 -> 调用 libt3sdk.so 进行验证 -> 验证成功才进入 MainActivity
 */
public class LoginActivity extends Activity
        implements View.OnClickListener, Runnable, Handler.Callback {

    private EditText etKami;
    private Button btnLogin;
    private ProgressBar progressBar;
    private TextView tvStatus;
    private Handler mainHandler;
    private String pendingKami;
    private String[] verifyResult;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mainHandler = new Handler(Looper.getMainLooper(), this);
        buildUI();
    }

    private void buildUI() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        int pad = dp(40);
        root.setPadding(pad, pad, pad, pad);

        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT);

        TextView tvTitle = new TextView(this);
        tvTitle.setText("T3 网络验证");
        tvTitle.setTextSize(24);
        tvTitle.setGravity(Gravity.CENTER);
        tvTitle.setPadding(0, 0, 0, dp(30));
        root.addView(tvTitle, lp);

        etKami = new EditText(this);
        etKami.setHint("请输入卡密");
        etKami.setSingleLine(true);
        etKami.setPadding(dp(16), dp(12), dp(16), dp(12));
        root.addView(etKami, lp);

        tvStatus = new TextView(this);
        tvStatus.setGravity(Gravity.CENTER);
        tvStatus.setPadding(0, dp(12), 0, dp(12));
        tvStatus.setTextColor(0xFF666666);
        root.addView(tvStatus, lp);

        btnLogin = new Button(this);
        btnLogin.setText("登 录");
        btnLogin.setOnClickListener(this);
        root.addView(btnLogin, lp);

        progressBar = new ProgressBar(this);
        progressBar.setVisibility(View.GONE);
        LinearLayout.LayoutParams pbLp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT);
        pbLp.gravity = Gravity.CENTER;
        root.addView(progressBar, pbLp);

        setContentView(root);
    }

    private int dp(int value) {
        return (int) (value * getResources().getDisplayMetrics().density + 0.5f);
    }

    @Override
    public void onClick(View v) {
        if (v == btnLogin) {
            doLogin();
        }
    }

    private void doLogin() {
        String kami = etKami.getText().toString().trim();
        if (kami.length() == 0) {
            Toast.makeText(this, "请输入卡密", Toast.LENGTH_SHORT).show();
            return;
        }
        pendingKami = kami;
        btnLogin.setEnabled(false);
        progressBar.setVisibility(View.VISIBLE);
        tvStatus.setText("正在验证，请稍候...");
        // 子线程执行验证（网络请求在 libt3sdk.so 中）
        new Thread(this).start();
    }

    /** 子线程入口 */
    @Override
    public void run() {
        verifyResult = T3Helper.verify(pendingKami);
        mainHandler.sendEmptyMessage(1);
    }

    /** 主线程回调 */
    @Override
    public boolean handleMessage(Message msg) {
        onVerifyResult();
        return true;
    }

    private void onVerifyResult() {
        String[] result = verifyResult;
        progressBar.setVisibility(View.GONE);
        btnLogin.setEnabled(true);

        if (result != null && "1".equals(result[0])) {
            tvStatus.setText("验证成功，正在进入...");
            Toast.makeText(this, "验证成功", Toast.LENGTH_SHORT).show();
            Intent intent = new Intent(this, MainActivity.class);
            intent.putExtra("kami", pendingKami);
            if (result.length > 2) {
                intent.putExtra("statecode", result[2]);
            }
            startActivity(intent);
            finish();
        } else {
            String msg = (result != null && result.length > 1) ? result[1] : "验证失败";
            tvStatus.setText(msg);
            Toast.makeText(this, msg, Toast.LENGTH_LONG).show();
        }
    }
}
