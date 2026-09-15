# T3 网络验证 SDK — 双版本完整对接文档

> 适用仓库: https://github.com/wanfeng090525/t3sdk
> 官网: https://www.t3yanzheng.com
> 更新日期: 2026-09-15

本仓库同时维护两个版本，均可独立接入 Android / Linux 应用：

| 版本 | 目录 | 语言 | 说明 |
|------|------|------|------|
| **版本A（上次版本）** | `versions/c-sdk/` | C | 完整功能版，暴露 20+ 接口，支持卡密/用户/QQ/远程变量/核心数据等 |
| **版本B（本次版本）** | `versions/official-cpp/` | C++ | 官方 C++ 源码精简版，只暴露 4 个核心接口（初始化/机器码/登录/心跳），接口面最小 |

两个版本的共同特点：

1. **所有 T3 后台凭证（调用码、APPKEY、RSA 公钥）都硬编码在 `.so` 文件内**，Java/C 调用端零凭证，反编译 APK 拿不到密钥。
2. 修改凭证只需改 JNI 绑定文件顶部的 `T3_*` 宏，重新编译 `.so`，**无需改动 Java 代码**。
3. GitHub Actions 可同时构建两个版本的 Android（4 架构）和 Linux `.so`，自动附加到 Release。

---

## 目录

1. [快速开始（5 分钟接入）](#1-快速开始5-分钟接入)
2. [凭证配置（改 .so 里的密钥）](#2-凭证配置改-so-里的密钥)
3. [版本B（官方 C++ 精简版）对接文档](#3-版本b官方-c-精简版对接文档)
4. [版本A（C 完整版）对接文档](#4-版本a-c-完整版对接文档)
5. [Android 集成步骤（两种版本通用）](#5-android-集成步骤两种版本通用)
6. [Linux 对接](#6-linux-对接)
7. [源码编译](#7-源码编译)
8. [GitHub Actions 自动构建](#8-github-actions-自动构建)
9. [接口与字段速查表](#9-接口与字段速查表)
10. [常见问题排查](#10-常见问题排查)

---

## 1. 快速开始（5 分钟接入）

### 1.1 获取 .so 文件

方式一（推荐）：直接下载 Release 产物

https://github.com/wanfeng090525/t3sdk/releases/latest

```
版本B: t3sdk-official-cpp-android.tar.gz   ← Android 4 架构 libt3sdk.so
       t3sdk-official-cpp-linux-x86_64.so  ← Linux 动态库
版本A: t3sdk-c-sdk-android.tar.gz          ← Android 4 架构 libt3sdk.so
       t3sdk-linux-x86_64.so               ← Linux 动态库
```

方式二：自己编译（见[第 7 节](#7-源码编译)）。

### 1.2 Android 最小集成（版本B示例）

```java
// 1. 把 libt3sdk.so 放到 app/src/main/jniLibs/{arm64-v8a,armeabi-v7a,x86,x86_64}/
// 2. 把 java/com/t3yanzheng/sdk/ 整个目录复制到 app/src/main/java/
// 3. AndroidManifest.xml 添加网络权限:
//    <uses-permission android:name="android.permission.INTERNET" />

import com.t3yanzheng.sdk.T3Verify;
import com.t3yanzheng.sdk.T3LoginResult;
import com.t3yanzheng.sdk.T3Result;

public class LoginActivity extends Activity {

    private T3Verify t3;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // 建议在子线程执行（网络请求）
        new Thread(() -> {
            boolean ok = doLogin("你的卡密");
            runOnUiThread(() -> {
                // 处理结果
            });
        }).start();
    }

    private boolean doLogin(String kami) {
        t3 = new T3Verify();
        if (!t3.init()) return false;              // 初始化（凭证在 .so 内，无需传参）

        String imei = T3Verify.getMachineCode();   // 获取机器码
        T3LoginResult r = t3.login(kami, imei);    // 卡密登录

        if (r.success) {
            // 登录成功！保存 statecode，用于心跳
            saveStatecode(r.statecode);
            // 登录后建议立即做一次心跳校验
            T3Result hb = t3.heartbeat(kami, r.statecode);
            if (!hb.success) {
                // 心跳失败（卡密被踢下线/到期），视为未登录
                return false;
            }
            return true;
        } else {
            // 登录失败，r.error 为失败原因
            return false;
        }
    }

    @Override
    protected void onDestroy() {
        if (t3 != null) t3.destroy();              // 释放资源
        super.onDestroy();
    }
}
```

### 1.3 定时心跳（防踢/防过期）

登录成功后，建议每 30~60 秒调用一次心跳：

```java
private void startHeartbeat(String kami, String statecode) {
    new Timer().schedule(new TimerTask() {
        @Override
        public void run() {
            T3Result hb = t3.heartbeat(kami, statecode);
            if (!hb.success) {
                // 心跳失败：卡密已到期/被踢下线，强制退出登录
                forceLogout();
            }
        }
    }, 30_000, 30_000);
}
```

---

## 2. 凭证配置（改 .so 里的密钥）

凭证全部编译在 `.so` 中，Java/C 端不出现任何密钥。

### 2.1 版本B — 修改 `versions/official-cpp/android/jni/t3sdk_jni.cpp` 顶部宏

```cpp
/* ============================================================
 * T3 后台凭证 - 直接编译进 .so，不会出现在 dex 中
 * 替换为你自己的 T3 后台配置后重新编译 .so 即可
 * ============================================================ */
#define T3_LOGIN_CODE     "813B2676E9690C89"    /* 单码登录调用码 */
#define T3_NOTICE_CODE    "EC56923E2FD91C99"    /* 获取程序公告调用码 */
#define T3_VERSION_CODE   "EA44543183C3F5D3"    /* 获取程序最新版本号调用码 */
#define T3_HEARTBEAT_CODE "9AB469F061FA45F4"    /* 单码卡密心跳验证调用码 */
#define T3_APPKEY         "d633f5e27c1b107cd2a1f98870787263"  /* 程序密钥APPKEY */
#define T3_RSA_PUBLIC_KEY "-----BEGIN PUBLIC KEY-----\n...-----END PUBLIC KEY-----"
```

### 2.2 版本A — 修改 `versions/c-sdk/src/t3sdk_jni.c` 顶部宏

```c
#define T3_LOGIN_CODE     "813B2676E9690C89"    /* 登录调用码 */
#define T3_NOTICE_CODE    "EC56923E2FD91C99"    /* 公告调用码 */
#define T3_VERSION_CODE   "EA44543183C3F5D3"    /* 版本调用码 */
#define T3_HEARTBEAT_CODE "9AB469F061FA45F4"    /* 心跳调用码 */
#define T3_APPKEY         "d633f5e27c1b107cd2a1f98870787263"  /* APPKEY */
/* 版本A 还支持 Base64 自定义字符集模式：
 * #define T3_BASE64_CHARSET "你的64字符自定义字符集" */
#define T3_RSA_PUBLIC_KEY "-----BEGIN PUBLIC KEY-----\n...-----END PUBLIC KEY-----"
```

### 2.3 加密模式说明

| 模式 | 版本B | 版本A | 说明 |
|------|-------|-------|------|
| RSA | ✅ 默认（`initRSA`） | ✅ 默认（`initRSA`） | 请求用 RSA 公钥加密，安全性高 |
| Base64 | ❌ | ✅（`init` + 自定义字符集） | 老后台可能使用，需填 `T3_BASE64_CHARSET` |

两个版本目前都编译为 **RSA 模式**（与官方示例一致）。如果你的 T3 后台是 Base64 模式：

- 版本B：把 `nativeInit` 里的 `verify->initRSA(...)` 改成 `verify->init(...)`（需同时在 t3sdk_jni.cpp 中提供 base64 字符集参数）。
- 版本A：Java 调用 `t3.init()`（Base64）代替 `t3.initRSA()`，并在 `t3sdk_jni.c` 中配置 `T3_BASE64_CHARSET`。

---

## 3. 版本B（官方 C++ 精简版）对接文档

### 3.1 目录结构

```
versions/official-cpp/
├── android/
│   └── jni/
│       ├── Android.mk           # ndk-build 构建脚本
│       ├── Application.mk       # ABI 与平台配置
│       ├── t3sdk_jni.cpp        # JNI 绑定 + 凭证宏（改这里）
│       └── t3sdk/
│           ├── t3sdk.cpp        # 官方 C++ 核心实现（仅优化 Android 机器码）
│           └── t3sdk.h          # 官方头文件
└── java/com/t3yanzheng/sdk/     # Java 封装（复制到你的项目）
    ├── T3Verify.java            # 主入口
    ├── T3LoginResult.java       # 登录结果
    └── T3Result.java            # 通用结果
```

### 3.2 Java API（`T3Verify`）

| 方法 | 说明 | 返回 |
|------|------|------|
| `T3Verify()` | 构造，创建 native 句柄 | - |
| `boolean init()` | 初始化 SDK（RSA 模式，凭证内置） | 是否成功 |
| `static String getMachineCode()` | 获取机器码（Android 网卡遍历 + 官方兜底） | 32 位大写 MD5 |
| `T3LoginResult login(String kami, String imei)` | 卡密登录 | 登录结果 |
| `T3Result heartbeat(String kami, String statecode)` | 卡密心跳（登录后周期调用） | 心跳结果 |
| `void destroy()` | 释放资源 | - |

### 3.3 结果类字段

**`T3LoginResult`**

| 字段 | 类型 | 说明 |
|------|------|------|
| `success` | boolean | 是否登录成功 |
| `error` | String | 失败原因（如“卡密错误”“已到期”） |
| `id` | String | 卡密/账号 ID |
| `endTime` | String | 到期时间 |
| `statecode` | String | 状态码（**登录成功后用于心跳**） |
| `recharge` | String | 充值相关信息 |
| `useTime` | String | 已用时间 |
| `amount` | String | 卡密金额/时长 |
| `available` | String | 剩余可用 |
| `imei` | String | 绑定的机器码 |
| `change` | String | 变更信息 |
| `core` | String | 卡密核心数据 |

**`T3Result`**

| 字段 | 类型 | 说明 |
|------|------|------|
| `success` | boolean | 是否成功 |
| `error` | String | 错误信息 |
| `msg` | String | 成功提示消息 |

### 3.4 完整调用流程

```
创建 T3Verify → init() → getMachineCode() → login(kami, imei)
    ├─ 成功 → heartbeat(kami, statecode) 定时调用
    └─ 失败 → 读取 error 提示用户
退出时 → destroy()
```

---

## 4. 版本A（C 完整版）对接文档

### 4.1 目录结构

```
versions/c-sdk/
├── include/t3sdk.h              # C API 头文件
├── src/
│   ├── t3sdk.c                  # C 核心实现（网络/机器码/MD5/RSA/Base64）
│   └── t3sdk_jni.c              # JNI 绑定 + 凭证宏（改这里）
├── android/
│   ├── jni/
│   │   ├── Android.mk           # ndk-build 构建脚本
│   │   └── Application.mk
│   └── CMakeLists.txt           # 备用 CMake 构建（旧结构示例）
├── java/com/t3yanzheng/sdk/     # Java 封装（完整版）
├── java_login/                  # 登录 Activity 示例源码
├── test/test.c                  # Linux 测试程序
└── Makefile                     # Linux 构建脚本
```

### 4.2 Java API（完整版，比版本B多 16 个接口）

**卡密验证**

| 方法 | 说明 |
|------|------|
| `boolean init()` | 初始化（Base64 模式） |
| `boolean initRSA()` | 初始化（RSA 模式，推荐） |
| `void setCode(String field, String code)` | 动态设置其他接口的调用码 |
| `static String getMachineCode()` | 获取机器码 |
| `T3LoginResult login(String kami, String imei)` | 卡密登录 |
| `T3QueryResult queryKami(String kami)` | 查询卡密信息 |
| `T3Result heartbeat(String kami, String statecode)` | 卡密心跳 |

**数据与内容**

| 方法 | 说明 |
|------|------|
| `T3NoticeResult getNotice()` | 获取程序公告 |
| `T3VersionResult getLatestVersion()` | 获取最新版本号 |
| `T3UpdateResult checkUpdate(String ver)` | 检查更新（传入当前版本号） |
| `T3CloudDocResult getCloudDoc(String token)` | 获取云端文档 |
| `T3AppSignResult appSign(String autograph)` | 应用签名验证 |

**用户体系**

| 方法 | 说明 |
|------|------|
| `T3Result userRegister(String user, String pass, String email)` | 用户注册 |
| `T3LoginResult userLogin(String user, String pass, String imei)` | 用户登录 |
| `T3Result userHeartbeat(String user, String pass, String statecode)` | 用户心跳 |
| `T3Result heartbeatAny(String statecode)` | 任意状态码心跳（不需要卡密） |

**设备与安全**

| 方法 | 说明 |
|------|------|
| `T3Result unbindKami(String kami, String imei)` | 解绑卡密（需在后台开启） |
| `T3Result disableKami(String kami)` | 封禁卡密 |

**在线数量**

| 方法 | 说明 |
|------|------|
| `T3OnlineResult getOnlineKamiCount()` | 在线卡密数 |
| `T3OnlineResult getOnlineUserCount()` | 在线用户数 |

### 4.3 扩展 C API（`include/t3sdk.h`）

JNI 只暴露了常用接口，完整 C API 还包含（可按需在 `t3sdk_jni.c` 中添加 JNI 封装后使用）：

- QQ 登录体系：`t3verify_qq_login` / `t3verify_bind_qq` / `t3verify_qq_heartbeat` / `t3verify_unbind_qq` 等
- 远程变量：`t3verify_get_variable_by_kami` / `t3verify_modify_variable_by_user` 等
- 核心数据：`t3verify_get_core_by_kami` / `t3verify_modify_core_by_user` 等
- 充值相关：`t3verify_recharge` / `t3verify_kami_recharge`
- 密码修改：`t3verify_change_password` / `t3verify_user_cancel`
- 解绑/封禁：`t3verify_unbind_user` / `t3verify_ip_unbind_kami` / `t3verify_ip_unbind_user` / `t3verify_disable_user`

> 需要哪个接口，按 `t3sdk_jni.c` 里现有函数的模式（创建结果对象 + 填字段）补一个 JNI 函数即可。

### 4.4 结果类

版本A 比版本B 多以下结果类，字段与 T3 官方字段一一对应：

- `T3QueryResult`：state / use / id / useTime / endTime / lineTime / line / amount / available
- `T3NoticeResult`：notice
- `T3VersionResult`：version
- `T3UpdateResult`：hasUpdate / ver / version / uplog / upurl / msg
- `T3CloudDocResult`：content
- `T3AppSignResult`：msg / autograph / time
- `T3OnlineResult`：count

---

## 5. Android 集成步骤（两种版本通用）

### 5.1 放入 .so 文件

```
app/src/main/jniLibs/
├── arm64-v8a/libt3sdk.so
├── armeabi-v7a/libt3sdk.so
├── x86/libt3sdk.so
└── x86_64/libt3sdk.so
```

### 5.2 复制 Java 封装

把对应版本的 `java/com/t3yanzheng/sdk/` 整个目录复制到你的 `app/src/main/java/` 下。

### 5.3 AndroidManifest.xml

```xml
<uses-permission android:name="android.permission.INTERNET" />
<!-- 机器码获取（可选，部分设备需要） -->
<uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
```

### 5.4 登录 Activity 完整示例（版本A 与版本B 写法一致）

```java
package com.example.myapp;

import android.app.Activity;
import android.os.Bundle;
import android.widget.Toast;
import com.t3yanzheng.sdk.T3Verify;
import com.t3yanzheng.sdk.T3LoginResult;
import com.t3yanzheng.sdk.T3Result;

public class LoginActivity extends Activity {
    private T3Verify t3;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        new Thread(() -> {
            boolean ok = doLogin("你的卡密");
            runOnUiThread(() -> Toast.makeText(this,
                    ok ? "登录成功" : "登录失败", Toast.LENGTH_LONG).show());
        }).start();
    }

    private boolean doLogin(String kami) {
        t3 = new T3Verify();
        if (!t3.init()) return false;
        String imei = T3Verify.getMachineCode();
        T3LoginResult r = t3.login(kami, imei);
        if (r.success) {
            T3Result hb = t3.heartbeat(kami, r.statecode);
            return hb.success;
        }
        return false;
    }

    @Override
    protected void onDestroy() {
        if (t3 != null) t3.destroy();
        super.onDestroy();
    }
}
```

---

## 6. Linux 对接

### 6.1 版本A（C）

```bash
# 编译（在仓库内）
cd versions/c-sdk
make shared          # 生成 libt3sdk.so（或直接 gcc -shared -fPIC -O2 -o libt3sdk.so src/t3sdk.c -I include）
make test            # 编译测试程序（可选）

# 链接到你的程序
gcc -I versions/c-sdk/include your_app.c -L versions/c-sdk -lt3sdk -o your_app
LD_LIBRARY_PATH=versions/c-sdk ./your_app
```

### 6.2 版本B（官方 C++）

```bash
# 编译
cd versions/official-cpp/android/jni
g++ -std=c++17 -shared -fPIC -O2 -o libt3sdk.so t3sdk/t3sdk.cpp -I .

# 链接到你的程序
g++ -std=c++17 your_app.cpp -L . -lt3sdk -o your_app
LD_LIBRARY_PATH=. ./your_app
```

### 6.3 直接调用 C 函数（绕过 JNI，版本A）

```c
#include <stdio.h>
#include "t3sdk.h"

int main(void) {
    T3Verify verify;
    T3LoginResult result;

    if (t3verify_init_rsa(&verify,
            "登录调用码", "公告调用码", "版本调用码", "心跳调用码",
            "APPKEY", "RSA公钥") != 0) {
        printf("初始化失败\n");
        return 1;
    }

    char machine[64] = {0};
    get_machine_code(machine);
    printf("机器码: %s\n", machine);

    if (t3verify_login(&verify, "卡密", machine, &result) == 0) {
        printf("登录成功, 到期: %s\n", result.end_time);
    } else {
        printf("登录失败: %s\n", result.error);
    }
    return 0;
}
```

---

## 7. 源码编译

### 7.1 环境准备

- Android NDK r27+（含 ndk-build）
- Linux: gcc / g++ 支持 C99 / C++17

### 7.2 构建 Android .so（4 架构）

```bash
# 版本A
cd versions/c-sdk/android
$ANDROID_NDK_HOME/ndk-build \
  NDK_PROJECT_PATH=. \
  APP_BUILD_SCRIPT=jni/Android.mk \
  NDK_APPLICATION_MK=jni/Application.mk
# 产物: libs/{arm64-v8a,armeabi-v7a,x86,x86_64}/libt3sdk.so

# 版本B
cd versions/official-cpp/android
$ANDROID_NDK_HOME/ndk-build \
  NDK_PROJECT_PATH=. \
  APP_BUILD_SCRIPT=jni/Android.mk \
  NDK_APPLICATION_MK=jni/Application.mk
```

### 7.3 构建 Linux .so

```bash
# 版本A
cd versions/c-sdk
gcc -shared -fPIC -O2 -o t3sdk-linux-x86_64.so src/t3sdk.c -I include

# 版本B
cd versions/official-cpp/android/jni
g++ -std=c++17 -shared -fPIC -O2 -o ../../../t3sdk-official-cpp-linux-x86_64.so \
  t3sdk/t3sdk.cpp -I .
```

---

## 8. GitHub Actions 自动构建

配置文件: `.github/workflows/build.yml`

### 8.1 触发方式

1. **推送 tag**（推荐）：`git tag vX.X.X && git push origin vX.X.X`
2. **手动**：仓库 Actions 页面 → 选择 `构建两个版本 T3 SDK` → Run workflow

### 8.2 构建内容

单 job 顺序执行，一次构建输出 4 个产物并附加到 Release：

| 产物 | 来源版本 | 内容 |
|------|----------|------|
| `t3sdk-c-sdk-android.tar.gz` | 版本A | Android 4 架构 libt3sdk.so |
| `t3sdk-linux-x86_64.so` | 版本A | Linux 动态库 |
| `t3sdk-official-cpp-android.tar.gz` | 版本B | Android 4 架构 libt3sdk.so |
| `t3sdk-official-cpp-linux-x86_64.so` | 版本B | Linux 动态库 |

同时上传 `t3sdk-both-versions` 工件（Artifacts，供在 Actions 页面下载）。

### 8.3 Release 自动发布

推送 tag 时，工作流自动 `gh release create` + `gh release upload`，把 4 个产物附加到对应 tag 的 Release。发布页：

https://github.com/wanfeng090525/t3sdk/releases

---

## 9. 接口与字段速查表

### 9.1 版本B 接口面（4 个）

```
init() → boolean
getMachineCode() → String
login(kami, imei) → T3LoginResult
heartbeat(kami, statecode) → T3Result
```

### 9.2 版本A 接口面（20 个）

```
init() / initRSA() / setCode(field, code) / getMachineCode()
login / queryKami / heartbeat
getNotice / getLatestVersion / checkUpdate / getCloudDoc / appSign
userRegister / userLogin / userHeartbeat / heartbeatAny
unbindKami / disableKami
getOnlineKamiCount / getOnlineUserCount
```

### 9.3 登录结果字段含义速查

| 字段 | 含义 |
|------|------|
| `success` | 是否成功 |
| `error` | 失败原因文本 |
| `id` | 卡密/账号标识 |
| `endTime` | 到期时间 |
| `statecode` | 状态码（用于心跳） |
| `useTime` | 已使用时间 |
| `amount` | 卡密面额/时长 |
| `available` | 剩余可用 |
| `imei` | 绑定机器码 |
| `core` | 卡密核心数据 |

---

## 10. 常见问题排查

| 现象 | 原因 | 解决 |
|------|------|------|
| `Response is not valid JSON format` | 凭证错误 / 加密模式与后台不一致 | 1) 核对 T3 后台的调用码与 APPKEY；2) 确认后台是 RSA 还是 Base64 模式，切换到对应 init 方法 |
| 机器码为空 | Android 网卡接口名与桌面版不同 | 已内置修复：遍历 wlan0/rmnet0 等接口 + 失败兜底 `00:00:00:00:00:00` 的 MD5，机器码永不为空 |
| APK 闪退 | jniLibs 架构不全 / 缺 .so | 4 个 ABI 目录都放 libt3sdk.so；确认 `System.loadLibrary("t3sdk")` 前已复制 Java 包 |
| `UnsatisfiedLinkError` | .so 与 Java 类不匹配 | 版本A 用 `versions/c-sdk/java`，版本B 用 `versions/official-cpp/java`，不要混用 |
| 心跳失败/被踢下线 | 卡密到期或后台强制下线 | 登录成功后必须周期性调用 heartbeat，失败则退出登录 |
| 修改凭证后没生效 | 只改了 Java，没改 .so | 凭证在 `t3sdk_jni.c` / `t3sdk_jni.cpp` 顶部宏，改完必须重新编译 .so |
| 两个版本冲突 | 同时集成 A 和 B 的 Java 包 | 二选一；如需同时使用，用不同包名并改 JNI 函数名前缀 |

---

## 附：目录文件索引

```
t3sdk/
├── .github/workflows/build.yml          # GitHub Actions 双版本构建脚本
├── versions/
│   ├── c-sdk/                           # 版本A - C 完整版
│   │   ├── include/t3sdk.h              #   C API 头文件
│   │   ├── src/t3sdk.c                  #   C 核心实现
│   │   ├── src/t3sdk_jni.c              #   JNI 绑定 + 凭证宏
│   │   ├── android/jni/Android.mk       #   NDK 构建脚本
│   │   ├── java/com/t3yanzheng/sdk/     #   Java 封装（完整版）
│   │   ├── java_login/                  #   登录 Activity 示例
│   │   ├── test/test.c                  #   Linux 测试程序
│   │   └── Makefile                     #   Linux 构建脚本
│   └── official-cpp/                    # 版本B - 官方 C++ 精简版
│       ├── android/jni/
│       │   ├── t3sdk_jni.cpp            #   JNI 绑定 + 凭证宏
│       │   ├── t3sdk/t3sdk.cpp          #   官方 C++ 核心实现
│       │   ├── t3sdk/t3sdk.h            #   官方头文件
│       │   ├── Android.mk               #   NDK 构建脚本
│       │   └── Application.mk
│       └── java/com/t3yanzheng/sdk/     #   Java 封装（精简版）
└── t3sdk.keystore                       # 示例签名 keystore
```
