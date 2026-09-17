# T3 验证 SDK - 官方 C++ 版对接文档（版本B / versions/official-cpp）

> 适用版本：v3.1.4.1（回退自 v3.1.4）
> 官网：https://www.t3yanzheng.com

## 1. SDK 简介

基于官方 `T3Example_AndroidJNI` 源码的精简版 SDK，提供 **两种调用方式**：

1. **Android Java 方式**（推荐）：通过 JNI 桥接层 `t3sdk_jni.cpp` 暴露全部能力，Java 端使用 `com.t3yanzheng.sdk.T3Verify` 类
2. **原生 C++ 方式**：直接调用 `T3Verify` 类（`t3sdk.h`），适用于 Linux 或 NDK 原生工程

特点：
- 纯 C++ 实现，不依赖 OpenSSL，自带 MD5 / 自定义 Base64 / RSA（公钥解析）实现
- 凭证（调用码 / APPKEY / RSA 公钥）**直接编译进 `.so`**，不在 dex/APK 中泄露
- 内置 6 台官方服务器自动探测切换

## 2. Android 集成步骤

### 2.1 获取 `.so`

编译 4 架构产物（或直接从 Release 下载 `t3sdk-official-cpp-android.tar.gz`）：

```bash
export ANDROID_NDK_HOME=/path/to/ndk
cd versions/official-cpp/android
$ANDROID_NDK_HOME/ndk-build \
  NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=jni/Android.mk NDK_APPLICATION_MK=jni/Application.mk
# 产物: libs/{arm64-v8a,armeabi-v7a,x86,x86_64}/libt3sdk.so
```

### 2.2 放入工程

把 `.so` 按架构复制到 App 工程：

```
app/src/main/jniLibs/
├── arm64-v8a/libt3sdk.so
├── armeabi-v7a/libt3sdk.so
├── x86/libt3sdk.so
└── x86_64/libt3sdk.so
```

复制以下 Java 类到你的工程（包名 `com.t3yanzheng.sdk`）：

```
T3Verify.java         SDK 主类（全部功能的 Java 封装）
T3Result.java         通用结果（success/error/msg）
T3LoginResult.java    登录结果
T3NoticeResult.java   公告结果
T3VersionResult.java  版本结果
T3QueryResult.java    卡密查询结果
T3UpdateResult.java   更新检查结果
T3VariableResult.java 远程变量结果
T3CloudDocResult.java 云文档结果
T3CoreResult.java     核心数据结果
T3OnlineResult.java   在线数量结果
T3AppSignResult.java  程序签名结果
```

`T3Verify.java` 的 `static { System.loadLibrary("t3sdk"); }` 会自动加载 `.so`，无需额外配置。

### 2.3 配置你自己的凭证（关键）

凭证硬编码在 [t3sdk_jni.cpp](versions/official-cpp/android/jni/t3sdk_jni.cpp) 顶部的宏中，**替换后重新编译 `.so`**：

```cpp
#define T3_LOGIN_CODE     "你的单码登录调用码"
#define T3_NOTICE_CODE    "你的公告调用码"
#define T3_VERSION_CODE   "你的版本调用码"
#define T3_HEARTBEAT_CODE "你的心跳调用码"
#define T3_APPKEY         "你的APPKEY"
#define T3_RSA_PUBLIC_KEY "-----BEGIN PUBLIC KEY-----\n...-----END PUBLIC KEY-----"
```

其余功能（查询/用户/QQ/变量/核心/在线等）的调用码在 `t3sdk_jni.cpp` 的 `nativeInit` 中通过 `setCode` 设置，同样改为你自己的值。

## 3. Java 方式功能调用

### 3.1 初始化与机器码

```java
T3Verify t3 = new T3Verify();
boolean ok = t3.init();                 // 凭证已内置在 .so，无需传参
String machineCode = T3Verify.getMachineCode();  // 获取机器码
```

### 3.2 卡密体系（单码）

```java
// 登录
T3LoginResult r = t3.login(kami, machineCode);
if (r != null && r.success) {
    String statecode = r.statecode;     // 状态码（心跳用）
    String endTime  = r.endTime;        // 到期时间
    String core     = r.core;           // 卡密核心数据
} else {
    String err = r != null ? r.error : "网络错误";
}

// 心跳（登录成功后每 60 秒调一次）
T3Result hb = t3.heartbeat(kami, r.statecode);
if (hb != null && hb.success) { /* 在线状态有效 */ }

// 查询卡密信息
T3QueryResult q = t3.queryKami(kami);

// 卡密充值（sourceKami 给 targetKami 充）
T3Result kr = t3.kamiRecharge(targetKami, sourceKami);
```

### 3.3 用户体系

```java
T3Result reg = t3.userRegister(user, pass, email);       // 注册
T3LoginResult ul = t3.userLogin(user, pass, machineCode); // 登录
T3Result uh = t3.userHeartbeat(user, pass, ul.statecode); // 心跳
T3Result cp = t3.changePassword(user, oldpass, newpass);  // 改密码
T3Result uc = t3.userCancel(user, pass);                  // 注销账号
T3Result rc = t3.recharge(user, card);                    // 充值
```

### 3.4 QQ 体系

```java
T3LoginResult ql = t3.qqLogin(openid, accessToken);                 // QQ登录
T3Result bq = t3.bindQq(user, pass, openid, accessToken);           // 账号绑定QQ
T3Result uq = t3.unbindQq(user, pass);                              // 账号解绑QQ
T3Result qh = t3.qqHeartbeat(openid, accessToken, ql.statecode);    // QQ心跳
```

### 3.5 设备与安全（解绑/禁用）

```java
T3Result un = t3.unbindKami(kami, machineCode);       // 卡密解绑机器码
T3Result uu = t3.unbindUser(user, pass, machineCode); // 用户解绑机器码
T3Result ipk = t3.ipUnbindKami(kami);                 // 卡密IP解绑
T3Result ipu = t3.ipUnbindUser(user, pass);           // 用户IP解绑
T3Result dk = t3.disableKami(kami);                   // 禁用卡密
T3Result du = t3.disableUser(user, pass);             // 禁用用户
```

### 3.6 远程变量 / 核心数据

```java
T3VariableResult var = t3.getVariableByKami(kami, "valueid", "valuename");
T3VariableResult varu = t3.getVariableByUser(user, pass, "valueid", "valuename");
T3VariableResult varq = t3.getVariableByQq(openid, accessToken, "valueid", "valuename");
String value = var.value;

T3Result mv = t3.modifyVariableByKami(kami, "valueid", "新内容");
T3Result mvu = t3.modifyVariableByUser(user, pass, "valueid", "新内容");

T3CoreResult core = t3.getCoreByKami(kami);                 // 读取核心数据
T3CoreResult coreu = t3.getCoreByUser(user, pass);
T3CoreResult coreq = t3.getUserCoreByQq(openid, accessToken);
T3Result mc = t3.modifyCoreByKami(kami, "新核心数据");
T3Result mcu = t3.modifyCoreByUser(user, pass, "新核心数据");
T3Result mcq = t3.modifyUserCoreByQq(openid, accessToken, "新核心数据");
```

### 3.7 数据与内容（公告/版本/更新/云文档/签名/在线数）

```java
T3NoticeResult n = t3.getNotice();                     // 公告 → n.notice
T3VersionResult v = t3.getLatestVersion();             // 最新版本 → v.version
T3UpdateResult up = t3.checkUpdate(v.version);         // 检查更新
// up.success && up.hasUpdate==true → 有更新: up.ver / up.uplog / up.upurl
T3CloudDocResult doc = t3.getCloudDoc(token);          // 云文档 → doc.content
T3AppSignResult sg = t3.appSign("待签名内容");          // 程序签名 → sg.autograph / sg.time
T3OnlineResult okc = t3.getOnlineKamiCount();          // 在线卡密数 → okc.count
T3OnlineResult ouc = t3.getOnlineUserCount();          // 在线用户数 → ouc.count
T3Result hany = t3.heartbeatAny(statecode);            // 统一心跳（任意登录方式）
```

### 3.8 生命周期

```java
@Override
protected void onDestroy() {
    super.onDestroy();
    t3.destroy();   // 释放 native 资源
}
```

### 3.9 线程说明

所有网络调用是同步阻塞的，**必须在子线程执行，不能在主线程调用**：

```java
ExecutorService executor = Executors.newSingleThreadExecutor();
Handler mainHandler = new Handler(Looper.getMainLooper());

executor.execute(() -> {
    T3LoginResult r = t3.login(kami, machineCode);
    mainHandler.post(() -> {
        // 更新 UI
    });
});
```

## 4. 原生 C++ 方式功能调用

直接包含 `t3sdk/t3sdk.h`，用法与 Java 一一对应：

```cpp
#include "t3sdk/t3sdk.h"
#include <cstdio>

int main() {
    // 1. 初始化（Base64 模式）
    T3Verify t3;
    if (!t3.init("登录调用码","公告调用码","版本调用码","心跳调用码",
                 "APPKEY", "自定义Base64字符集")) {
        printf("初始化失败\n");
        return -1;
    }
    // 或 RSA 模式:
    // t3.initRSA("登录调用码","公告调用码","版本调用码","心跳调用码","APPKEY", "公钥PEM");

    // 2. 设置其余调用码
    t3.setCode("unbind", "解绑调用码");
    t3.setCode("checkupdate", "检查更新调用码");
    t3.setCode("getvariable", "获取变量调用码");
    // ... 与 C 版一致的字段名

    // 3. 机器码
    std::string machine = getMachineCode();

    // 4. 登录
    T3LoginResult login = t3.login("卡密", machine);
    if (login.success) {
        printf("登录成功, 到期: %s, 状态码: %s\n",
               login.end_time.c_str(), login.statecode.c_str());
    } else {
        printf("登录失败: %s\n", login.error.c_str());
        return -1;
    }

    // 5. 心跳
    T3Result hb = t3.heartbeat("卡密", login.statecode);
    if (hb.success) printf("心跳正常\n");

    // 6. 公告 / 版本 / 更新
    T3NoticeResult notice = t3.getNotice();
    T3VersionResult ver = t3.getLatestVersion();
    T3UpdateResult up = t3.checkUpdate(ver.version);
    if (up.success && up.hasUpdate) {
        printf("有更新: %s\n", up.ver.c_str());
    }

    // 7. 解绑
    T3Result unb = t3.unbindKami("卡密", machine);
    if (unb.success) printf("解绑成功\n");

    return 0;
}
```

Linux 编译：

```bash
cd versions/official-cpp/android/jni
g++ -std=c++17 -O2 -o my_app t3sdk/t3sdk.cpp my_app.cpp -I .
```

### 原生 C++ 完整功能对应表

| 分类 | C++ 方法 |
|---|---|
| 初始化 | `init()`（Base64）、`initRSA()`、`setCode(field, code)` |
| 卡密 | `login(kami, imei)`、`queryKami(kami)`、`heartbeat(kami, statecode)`、`kamiRecharge(target, source)` |
| 用户 | `userRegister(u,p,email)`、`userLogin(u,p,imei)`、`userHeartbeat(u,p,statecode)`、`changePassword(u,old,new)`、`userCancel(u,p)`、`recharge(u,card)` |
| QQ | `qqLogin(openid, token)`、`bindQQ(u,p,openid,token)`、`qqHeartbeat(openid,token,statecode)`、`unbindQq(u,p)` |
| 设备安全 | `unbindKami(kami,imei)`、`unbindUser(u,p,imei)`、`ipUnbindKami(kami)`、`ipUnbindUser(u,p)`、`disableKami(kami)`、`disableUser(u,p)` |
| 远程变量 | `getVariableByKami/getVariableByUser/getVariableByQq(...)`、`modifyVariableByKami/modifyVariableByUser(...)` |
| 核心数据 | `getCoreByKami/getCoreByUser/getUserCoreByQq(...)`、`modifyCoreByKami/modifyCoreByUser/modifyUserCoreByQq(...)` |
| 数据内容 | `getNotice()`、`getLatestVersion()`、`checkUpdate(ver)`、`getCloudDoc(token)`、`appSign(autograph)` |
| 在线/心跳 | `getOnlineKamiCount()`、`getOnlineUserCount()`、`heartbeatAny(statecode)` |

## 5. Java 结果对象字段速查

| Java 类 | 字段（`success=true` 时） |
|---|---|
| `T3Result` | `msg` |
| `T3LoginResult` | `id`、`endTime`、`statecode`、`recharge`、`useTime`、`amount`、`available`、`imei`、`change`、`core` |
| `T3NoticeResult` | `notice` |
| `T3VersionResult` | `version` |
| `T3QueryResult` | `state`、`use`、`id`、`useTime`、`endTime`、`lineTime`、`line`、`amount`、`available` |
| `T3UpdateResult` | `hasUpdate`、`ver`、`version`、`uplog`、`upurl`、`msg` |
| `T3VariableResult` | `value` |
| `T3CloudDocResult` | `content` |
| `T3CoreResult` | `core` |
| `T3OnlineResult` | `count` |
| `T3AppSignResult` | `msg`、`autograph`、`time` |

所有失败情况：`success=false`，原因在 `error` 字段。

## 6. 常见问题

1. **`System.loadLibrary` 崩溃 / `UnsatisfiedLinkError`**：确认 4 个 ABI 目录的 `.so` 都放入了 `jniLibs`，且架构与设备匹配。
2. **登录提示凭证错误**：`t3sdk_jni.cpp` 顶部的宏还是默认凭证，替换成你自己的调用码/APPKEY/公钥后重新编译 `.so`。
3. **网络请求不能放主线程**：SDK 是同步阻塞的，主线程调用会 ANR。
4. **机器码获取失败**：需要网络权限 `android.permission.INTERNET` 和 `ACCESS_NETWORK_STATE`（获取网卡信息）。
5. **换设备登录**：卡密绑定机器码，先 `unbindKami` 解绑或使用 IP 解绑，再在新设备登录。
6. **如何验证 .so 是否包含修复代码**：`python3 scripts/check_so.py versions/official-cpp/android/libs` 会检查 4 架构产物是否包含 chunked/content-length 等关键实现。
