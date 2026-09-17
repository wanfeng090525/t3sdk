# libt3sdk.so 对接文档（Android JNI / 官方 C++ 版）

> 适用版本：v3.1.4.1（回退自 v3.1.4）
> 源码：`versions/official-cpp/` | 官网：https://www.t3yanzheng.com
> 产物：`t3sdk-official-cpp-android.tar.gz`（4 架构 .so）、`t3sdk-official-cpp-linux-x86_64.so`

## 1. libt3sdk.so 是什么

`libt3sdk.so` 是 T3 网络验证的 **Android JNI 动态库**，内部完成：
- 网络通信（内置 6 台官方服务器，自动探测切换）
- 数据加解密（RSA 模式，公钥内置）
- 签名（MD5 + 自定义 Base64 + 时间戳）

对外通过 **JNI 接口**（导出符号 `Java_com_t3yanzheng_sdk_T3Verify_*`）提供服务。Java 侧只需：

```java
static { System.loadLibrary("t3sdk"); }   // T3Verify.java 已内置
```

然后调用 `T3Verify` 类的方法即可，**所有凭证都已编译在 .so 里，Java 层不需要也不包含任何调用码/APPKEY/公钥**。

## 1.1 APK 实际对接方式（深度分析自 My_App_T3SDK.apk）

> 以下流程是反编译 APK 得到的真实用法，按此写你的 App 即可。

```java
public class LoginActivity extends Activity {
    private T3Verify t3;
    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private final Handler mainHandler = new Handler(Looper.getMainLooper());

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        t3 = new T3Verify();          // 构造时自动创建 native 句柄
        boolean ok = t3.init();       // 主线程 init（只加载内置凭证，不发网络请求）
        // 机器码必须在子线程获取
        executor.execute(() -> {
            final String machine = T3Verify.getMachineCode();   // static native
            mainHandler.post(() -> tvMachine.setText(machine));
        });
        btnLogin.setOnClickListener(v -> doLogin());
    }

    private void doLogin() {
        final String kami = etKami.getText().toString().trim();
        executor.execute(() -> {
            T3LoginResult r = t3.login(kami, T3Verify.getMachineCode());
            mainHandler.post(() -> {
                if (r != null && r.success) {
                    // ★ 保存登录信息，重启后用于自动登录和心跳
                    getSharedPreferences("t3_prefs", 0).edit()
                        .putString("kami", kami)
                        .putString("imei", T3Verify.getMachineCode())
                        .putString("statecode", r.statecode)
                        .putString("end_time", r.endTime).apply();
                    startActivity(new Intent(this, MainActivity.class));
                    finish();
                } else {
                    showError(r != null ? r.error : "登录失败");
                }
            });
        });
    }

    @Override protected void onDestroy() {
        super.onDestroy();
        if (t3 != null) { t3.destroy(); t3 = null; }
        executor.shutdown();
    }
}
```

```java
public class MainActivity extends Activity {
    private static final long HEARTBEAT_INTERVAL = 30000;  // ★ 实际就是 30 秒
    private T3Verify t3;
    private String kami, imei, statecode;
    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final Runnable heartbeatTask = new Runnable() {
        @Override public void run() {
            executor.execute(() -> {
                T3Result hb = t3.heartbeat(kami, statecode);   // 子线程心跳
                mainHandler.post(() -> tvHeartbeat.setText(hb != null && hb.success ? "正常" : "异常"));
            });
            mainHandler.postDelayed(this, HEARTBEAT_INTERVAL); // 循环调度
        }
    };

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        SharedPreferences sp = getSharedPreferences("t3_prefs", 0);
        kami = sp.getString("kami", "");
        imei = sp.getString("imei", "");
        statecode = sp.getString("statecode", "");
        if (TextUtils.isEmpty(kami)) { backToLogin(); return; }
        t3 = new T3Verify();
        t3.init();
        mainHandler.postDelayed(heartbeatTask, HEARTBEAT_INTERVAL);
    }

    // 公告/更新/解绑 全部在 executor 里调用：
    //   T3NoticeResult n = t3.getNotice();                 → 公告 n.notice
    //   T3VersionResult v = t3.getLatestVersion();
    //   T3UpdateResult  up = t3.checkUpdate(v.version);    → up.hasUpdate / up.ver / up.uplog
    //   T3Result unb = t3.unbindKami(kami, imei);          → 解绑

    @Override protected void onDestroy() {
        mainHandler.removeCallbacks(heartbeatTask);
        if (t3 != null) { t3.destroy(); t3 = null; }
        executor.shutdown();
        super.onDestroy();
    }
}
```

**关键点（与 APK 逐行核对）**：
- 心跳间隔 `30000`ms = **30 秒**（不是 60 秒），用 `postDelayed` 循环
- `statecode` 必须持久化（APK 用 `SharedPreferences("t3_prefs")`），重启后不需要重新登录也能直接心跳
- `login()` 的第二个参数、`unbindKami()` 的第二个参数、`heartbeat()` 的第二个参数都是**机器码 / statecode**，别传反
- 每次 `new T3Verify()` 用完必须 `destroy()`；`finalize()` 只是兜底，不要依赖

## 2. 内置凭证（你的 T3 后台配置，已在 .so 中，不要改动）

| 项 | 值 |
|---|---|
| 登录调用码 login | `813B2676E9690C89` |
| 公告调用码 notice | `EC56923E2FD91C99` |
| 版本调用码 version | `EA44543183C3F5D3` |
| 心跳调用码 heartbeat | `9AB469F061FA45F4` |
| APPKEY | `d633f5e27c1b107cd2a1f98870787263` |
| 加密模式 | RSA（公钥 PEM 已内置） |

其余功能调用码（`nativeInit` 中自动设置，也已在 .so 内）：

| 功能 | 调用码 | 功能 | 调用码 |
|---|---|---|---|
| 查询卡密 query | `A2AC50154CC51A76` | 卡密充值 kami_recharge | `A4634B5AC1F5341A` |
| 用户注册 register | `6B9EABFF00B80750` | 卡密解绑 unbind | `2CA5F5986D945633` |
| 用户登录 user_login | `ECCFEFE957984480` | IP解绑 ip_unbind | `C21B2219566669B9` |
| 用户心跳 user_heartbeat | `EF796F4016C97A66` | 禁用 disable | `CF41580160F0AE75` |
| QQ登录 qq_login | `B6652EB5C78F0641` | 检查更新 check_update | `890829CBEFA61A56` |
| 绑定QQ bind_qq | `BF0378334FF0F0F5` | 获取变量 get_variable | `A015ABD9403AC64A` |
| 修改密码 change_password | `B7C4CD0FAE40AE8C` | 修改变量 modify_variable | `78CFB294B32DC2A3` |
| 注销账号 user_cancel | `A71666D3D237E922` | 修改核心 modify_core | `F9CE457547B9DD0F` |
| 用户充值 recharge | `57559AB00DE6A9DE` | 卡密核心 get_kami_core | `8C58C52B3053820A` |
| 在线卡密 online_kami | `B61D29EBFA5E3CF5` | 用户核心 get_user_core | `CD79FE55F3CE48B2` |
| 在线用户 online_user | `0D46101793B42DD3` | 云文档 cloud_doc | `F50B362FC86CC38E` |
| 程序签名 app_sign | `BAB98433A890FFB0` | QQ心跳 qq_heartbeat | `77C9731065BAB2A4` |
| 按QQ取变量 qq_get_variable | `2CE5BD9B12733032` | 按QQ取核心 qq_get_core | `5EF62F8A7630DC9A` |
| 按QQ改核心 qq_modify_core | `ACA861E2C03E9385` | 解绑QQ qq_unbind | `8E113021CB8FE80E` |
| 统一心跳 heartbeat_any | `A0D682FB88F82D31` | | |

## 3. 集成步骤

### 3.1 放入 .so

把 `t3sdk-official-cpp-android.tar.gz` 解压出的 4 个架构 .so 放进 App 工程：

```
app/src/main/jniLibs/
├── arm64-v8a/libt3sdk.so
├── armeabi-v7a/libt3sdk.so
├── x86/libt3sdk.so
└── x86_64/libt3sdk.so
```

### 3.2 复制 Java 接口类

从仓库 `app/app/src/main/java/com/t3yanzheng/sdk/` 复制以下 13 个类（与 .so 的 JNI 接口一一对应）：

```
T3Verify.java         主类（生命周期 + 全部功能方法）
T3Helper.java         极简封装（verify / heartbeat，可选）
T3Result.java         通用结果（success / error / msg）
T3LoginResult.java    登录结果
T3QueryResult.java    卡密查询结果
T3NoticeResult.java   公告结果
T3VersionResult.java  版本结果
T3UpdateResult.java   更新检查结果
T3VariableResult.java 远程变量结果
T3CloudDocResult.java 云文档结果
T3CoreResult.java     核心数据结果
T3OnlineResult.java   在线数量结果
T3AppSignResult.java  程序签名结果
```

> 注意：这些类的**包名必须是 `com.t3yanzheng.sdk`**（JNI 导出符号按包名+类名+方法名生成，改包名会导致找不到符号）。

### 3.3 权限

`AndroidManifest.xml` 需要：

```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
```

## 4. .so 接口总览（JNI 导出 → Java 方法）

`.so` 导出的所有 JNI 接口与 `T3Verify` 的 Java 方法一一对应，Java 方法内部就是调用 .so：

| JNI 导出符号（.so 内） | Java 方法 | 返回类型 |
|---|---|---|
| `nativeCreate` | `new T3Verify()` 构造时自动调用 | `long` 句柄 |
| `nativeDestroy` | `destroy()` | `void` |
| `nativeInit` | `init()` | `boolean` |
| `nativeSetCode` | `setCode(field, code)` | `void` |
| `nativeGetMachineCode` | `getMachineCode()`（静态） | `String` |
| `nativeLogin` | `login(kami, imei)` | `T3LoginResult` |
| `nativeQueryKami` | `queryKami(kami)` | `T3QueryResult` |
| `nativeHeartbeat` | `heartbeat(kami, statecode)` | `T3Result` |
| `nativeKamiRecharge` | `kamiRecharge(target, source)` | `T3Result` |
| `nativeUserRegister` | `userRegister(user, pass, email)` | `T3Result` |
| `nativeUserLogin` | `userLogin(user, pass, imei)` | `T3LoginResult` |
| `nativeUserHeartbeat` | `userHeartbeat(user, pass, statecode)` | `T3Result` |
| `nativeRecharge` | `recharge(user, card)` | `T3Result` |
| `nativeChangePassword` | `changePassword(user, old, new)` | `T3Result` |
| `nativeUserCancel` | `userCancel(user, pass)` | `T3Result` |
| `nativeQqLogin` | `qqLogin(openid, accessToken)` | `T3LoginResult` |
| `nativeBindQq` | `bindQq(user, pass, openid, accessToken)` | `T3Result` |
| `nativeUnbindQq` | `unbindQq(user, pass)` | `T3Result` |
| `nativeQqHeartbeat` | `qqHeartbeat(openid, accessToken, statecode)` | `T3Result` |
| `nativeUnbindKami` | `unbindKami(kami, imei)` | `T3Result` |
| `nativeUnbindUser` | `unbindUser(user, pass, imei)` | `T3Result` |
| `nativeIpUnbindKami` | `ipUnbindKami(kami)` | `T3Result` |
| `nativeIpUnbindUser` | `ipUnbindUser(user, pass)` | `T3Result` |
| `nativeDisableKami` | `disableKami(kami)` | `T3Result` |
| `nativeDisableUser` | `disableUser(user, pass)` | `T3Result` |
| `nativeGetVariableByKami` | `getVariableByKami(kami, valueid, valuename)` | `T3VariableResult` |
| `nativeGetVariableByUser` | `getVariableByUser(user, pass, valueid, valuename)` | `T3VariableResult` |
| `nativeGetVariableByQq` | `getVariableByQq(openid, accessToken, valueid, valuename)` | `T3VariableResult` |
| `nativeModifyVariableByKami` | `modifyVariableByKami(kami, valueid, valuecontent)` | `T3Result` |
| `nativeModifyVariableByUser` | `modifyVariableByUser(user, pass, valueid, valuecontent)` | `T3Result` |
| `nativeGetCoreByKami` | `getCoreByKami(kami)` | `T3CoreResult` |
| `nativeGetCoreByUser` | `getCoreByUser(user, pass)` | `T3CoreResult` |
| `nativeGetUserCoreByQq` | `getUserCoreByQq(openid, accessToken)` | `T3CoreResult` |
| `nativeModifyCoreByKami` | `modifyCoreByKami(kami, core)` | `T3Result` |
| `nativeModifyCoreByUser` | `modifyCoreByUser(user, pass, core)` | `T3Result` |
| `nativeModifyUserCoreByQq` | `modifyUserCoreByQq(openid, accessToken, core)` | `T3Result` |
| `nativeGetNotice` | `getNotice()` | `T3NoticeResult` |
| `nativeGetLatestVersion` | `getLatestVersion()` | `T3VersionResult` |
| `nativeCheckUpdate` | `checkUpdate(ver)` | `T3UpdateResult` |
| `nativeGetCloudDoc` | `getCloudDoc(token)` | `T3CloudDocResult` |
| `nativeAppSign` | `appSign(autograph)` | `T3AppSignResult` |
| `nativeGetOnlineKamiCount` | `getOnlineKamiCount()` | `T3OnlineResult` |
| `nativeGetOnlineUserCount` | `getOnlineUserCount()` | `T3OnlineResult` |
| `nativeHeartbeatAny` | `heartbeatAny(statecode)` | `T3Result` |

## 5. 接口调用说明（完整）

> 所有接口都是**同步阻塞**的，必须在子线程调用。调用结果先判 `null`（网络/句柄异常），再判 `success`。

### 5.1 生命周期

```java
T3Verify t3 = new T3Verify();      // 内部调用 nativeCreate，创建句柄
boolean ok = t3.init();            // 内部 nativeInit：载入内置凭证并初始化
if (!ok) { /* .so 加载或初始化失败 */ }
// ... 使用 ...
t3.destroy();                      // 内部 nativeDestroy，释放 native 句柄
```

### 5.2 机器码

```java
String machine = T3Verify.getMachineCode();  // 静态方法，直接调 .so nativeGetMachineCode
```

### 5.3 卡密登录 + 心跳（核心流程）

```java
// 登录
T3LoginResult r = t3.login(kami, machine);
if (r != null && r.success) {
    // r.statecode → 心跳状态码
    // r.endTime    → 到期时间
    // r.imei       → 绑定机器码
    // r.core       → 卡密核心数据
    // r.id / r.recharge / r.useTime / r.amount / r.available / r.change
} else {
    String err = (r != null && r.error != null) ? r.error : "网络错误";
}

// 心跳（登录成功后每 30 秒调用一次，返回 false 表示在线状态失效）
T3Result hb = t3.heartbeat(kami, r.statecode);
if (hb != null && hb.success) { /* 在线 */ }
```

### 5.3.1 T3Helper（SDK 自带的极简封装，可选）

APK 的 sdk 包里还带一个 `T3Helper`，不想写线程就用它（它内部自建/销毁 `T3Verify`）：

```java
// 验证卡密：返回 { "1"/"0", 消息, statecode }
String[] r = T3Helper.verify(kami);
if ("1".equals(r[0])) { String statecode = r[2]; }

// 心跳：返回 boolean
boolean alive = T3Helper.heartbeat(kami, statecode);
```

### 5.4 卡密其他功能

```java
T3QueryResult q = t3.queryKami(kami);              // 查询卡密
// q.state/q.use/q.id/q.useTime/q.endTime/q.lineTime/q.line/q.amount/q.available
T3Result kr = t3.kamiRecharge(targetKami, sourceKami);  // 卡密充值
```

### 5.5 用户体系

```java
T3Result reg = t3.userRegister(user, pass, email);        // 注册
T3LoginResult ul = t3.userLogin(user, pass, machine);     // 登录（返回同 5.3）
T3Result uh = t3.userHeartbeat(user, pass, ul.statecode); // 心跳
T3Result cp = t3.changePassword(user, oldPass, newPass);  // 修改密码
T3Result uc = t3.userCancel(user, pass);                  // 注销账号
T3Result rc = t3.recharge(user, card);                    // 充值
```

### 5.6 QQ 体系

```java
T3LoginResult ql = t3.qqLogin(openid, accessToken);                 // QQ 登录
T3Result bq = t3.bindQq(user, pass, openid, accessToken);           // 账号绑定 QQ
T3Result uq = t3.unbindQq(user, pass);                              // 账号解绑 QQ
T3Result qh = t3.qqHeartbeat(openid, accessToken, ql.statecode);    // QQ 心跳
```

### 5.7 设备与安全

```java
T3Result un = t3.unbindKami(kami, machine);        // 卡密解绑机器码（换设备前用）
T3Result uu = t3.unbindUser(user, pass, machine);  // 用户解绑机器码
T3Result ipk = t3.ipUnbindKami(kami);              // 卡密 IP 解绑
T3Result ipu = t3.ipUnbindUser(user, pass);        // 用户 IP 解绑
T3Result dk = t3.disableKami(kami);                // 禁用卡密
T3Result du = t3.disableUser(user, pass);          // 禁用用户
```

### 5.8 远程变量（服务器端动态配置）

```java
// valueid=变量ID, valuename=变量名（T3 后台「变量管理」里配置）
T3VariableResult var = t3.getVariableByKami(kami, valueid, valuename);
T3VariableResult varu = t3.getVariableByUser(user, pass, valueid, valuename);
T3VariableResult varq = t3.getVariableByQq(openid, accessToken, valueid, valuename);
String value = var.value;   // 变量值

// 修改变量
T3Result mv  = t3.modifyVariableByKami(kami, valueid, valuecontent);
T3Result mvu = t3.modifyVariableByUser(user, pass, valueid, valuecontent);
```

### 5.9 核心数据（自定义加密存储）

```java
T3CoreResult core  = t3.getCoreByKami(kami);                 // 读卡密核心
T3CoreResult coreu = t3.getCoreByUser(user, pass);           // 读用户核心
T3CoreResult coreq = t3.getUserCoreByQq(openid, accessToken);// 读 QQ 核心
String c = core.core;   // 核心数据内容

T3Result mc  = t3.modifyCoreByKami(kami, core);              // 写卡密核心
T3Result mcu = t3.modifyCoreByUser(user, pass, core);        // 写用户核心
T3Result mcq = t3.modifyUserCoreByQq(openid, accessToken, core);  // 写 QQ 核心
```

### 5.10 公告 / 版本 / 更新 / 云文档 / 签名 / 在线数

```java
T3NoticeResult n = t3.getNotice();               // 程序公告 → n.notice
T3VersionResult v = t3.getLatestVersion();       // 最新版本号 → v.version
T3UpdateResult up = t3.checkUpdate(v.version);   // 检查更新
// up.hasUpdate==true 有更新: up.ver 新版本号 / up.uplog 更新日志 / up.upurl 下载地址
T3CloudDocResult doc = t3.getCloudDoc(token);    // 云文档 → doc.content
T3AppSignResult sg = t3.appSign(autograph);      // 程序签名 → sg.autograph / sg.time
T3OnlineResult okc = t3.getOnlineKamiCount();    // 在线卡密数 → okc.count
T3OnlineResult ouc = t3.getOnlineUserCount();    // 在线用户数 → ouc.count
T3Result hany = t3.heartbeatAny(statecode);      // 统一心跳（任意登录方式共用）
```

### 5.11 动态设置调用码（一般不需要）

```java
// 凭证已内置，正常无需调用；如需覆盖某个调用码：
t3.setCode("login", "813B2676E9690C89");
```

## 6. 完整示例（登录 → 心跳 → 公告 → 更新 → 解绑）

见第 1.1 节"APK 实际对接方式"，那是反编译自真实 APK 的完整流程（`LoginActivity` + `MainActivity`），直接照抄即可。核心顺序：

1. `new T3Verify()` → `init()`（主线程）
2. 子线程 `T3Verify.getMachineCode()` + `t3.login(kami, machine)`
3. 登录成功 → 持久化 `statecode`/`end_time`（`SharedPreferences("t3_prefs")`）
4. 每 **30 秒**子线程 `t3.heartbeat(kami, statecode)`
5. 功能调用全部放子线程；页面销毁时 `t3.destroy()`

## 7. 结果对象字段速查

| 结果类 | `success=true` 时可用字段 |
|---|---|
| `T3Result` | `msg`（失败时看 `error`） |
| `T3LoginResult` | `id`、`endTime`、`statecode`、`recharge`、`useTime`、`amount`、`available`、`imei`、`change`、`core` |
| `T3QueryResult` | `state`、`use`、`id`、`useTime`、`endTime`、`lineTime`、`line`、`amount`、`available` |
| `T3NoticeResult` | `notice` |
| `T3VersionResult` | `version` |
| `T3UpdateResult` | `hasUpdate`、`ver`、`version`、`uplog`、`upurl`、`msg` |
| `T3VariableResult` | `value` |
| `T3CloudDocResult` | `content` |
| `T3CoreResult` | `core` |
| `T3OnlineResult` | `count` |
| `T3AppSignResult` | `msg`、`autograph`、`time` |

**统一约定**：`success=false` 时原因在 `error` 字段；方法返回 `null` 表示句柄异常或网络失败。

## 8. 常见问题

1. **`UnsatisfiedLinkError`**：4 个 ABI 的 `.so` 必须放齐 `jniLibs/<abi>/`，Java 类包名必须是 `com.t3yanzheng.sdk`。
2. **`nativeInit` 返回 false**：`.so` 与 Java 类不匹配（版本不一致），或设备不兼容。
3. **网络调用 ANR**：所有 `.so` 接口是同步阻塞的，禁止在主线程调用。
4. **登录成功但马上失败**：检查心跳 `statecode` 是否来自最近一次登录返回，心跳过期需要重新登录。
5. **机器码获取失败**：缺网络权限，或设备无网卡信息。
6. **换设备登录**：先 `unbindKami(kami, machine)` 解绑（或用 `ipUnbindKami`），再在新设备登录。
