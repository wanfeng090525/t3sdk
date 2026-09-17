# T3 验证 SDK - C 语言版对接文档（版本A / versions/c-sdk）

> 适用版本：v3.1.4.1（回退自 v3.1.4）
> 官网：https://www.t3yanzheng.com

## 1. SDK 简介

C 语言实现的 T3 网络验证 SDK，纯 C 编写、不依赖 OpenSSL，自带 MD5、自定义 Base64、RSA 加密和 HTTP(S) 网络层。可用于 **Android（NDK 原生）** 和 **Linux** 平台。

- 服务器：默认内置 6 台服务器地址，自动随机打乱并逐个探测，无需手动配置
- 编码：支持 Base64（自定义字符集）和 RSA 双模式
- 机器码：内置 `get_machine_code()`，生成设备唯一标识用于卡密绑定

## 2. 集成方式

### 2.1 Android（NDK）

把 `versions/c-sdk/android` 用 NDK 编译，产物为 4 架构的 `libt3sdk.so`：

```bash
export ANDROID_NDK_HOME=/path/to/ndk
cd versions/c-sdk/android
$ANDROID_NDK_HOME/ndk-build \
  NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=jni/Android.mk NDK_APPLICATION_MK=jni/Application.mk
# 产物在 libs/{arm64-v8a,armeabi-v7a,x86,x86_64}/libt3sdk.so
```

把对应架构的 `.so` 放进 App 的 `src/main/jniLibs/<abi>/` 目录，然后通过 C 代码直接调用（详见第 4 节）。

### 2.2 Linux

```bash
cd versions/c-sdk
gcc -shared -fPIC -O2 -o t3sdk-linux-x86_64.so src/t3sdk.c -I include
# 或静态链接到你的程序：
gcc -O2 -o my_app src/t3sdk.c include/t3sdk.h 你的源码.c -I include
```

## 3. 初始化与凭证配置

### 3.1 获取凭证

登录 T3 后台（https://www.t3yanzheng.com）获取：
- **调用码**：`login`（单码登录）、`notice`（公告）、`version`（最新版本）、`heartbeat`（心跳）等，每个功能对应一个调用码
- **APPKEY**：程序密钥
- **编码模式**：Base64（自定义字符集）或 RSA（公钥 PEM）

### 3.2 Base64 模式初始化

```c
T3Verify t3;
int ok = t3verify_init(&t3,
    "登录调用码",    /* login_code    */
    "公告调用码",    /* notice_code   */
    "版本调用码",    /* version_code  */
    "心跳调用码",    /* heartbeat_code*/
    "APPKEY",        /* appkey        */
    "自定义Base64字符集");  /* base64_charset */
if (ok != 0) { /* 初始化失败 */ }
```

### 3.3 RSA 模式初始化

```c
T3Verify t3;
int ok = t3verify_init_rsa(&t3,
    "登录调用码", "公告调用码", "版本调用码", "心跳调用码",
    "APPKEY", "-----BEGIN PUBLIC KEY-----\n...-----END PUBLIC KEY-----");
```

### 3.4 设置新增调用码

除 4 个基础调用码外，其余功能通过 `t3verify_set_code` 按字段名设置：

```c
t3verify_set_code(&t3, "query",        "查询卡密调用码");
t3verify_set_code(&t3, "register",     "用户注册调用码");
t3verify_set_code(&t3, "userlogin",    "用户登录调用码");
t3verify_set_code(&t3, "userheart",    "用户心跳调用码");
t3verify_set_code(&t3, "qqlogin",      "QQ登录调用码");
t3verify_set_code(&t3, "bindqq",       "绑定QQ调用码");
t3verify_set_code(&t3, "changepwd",    "修改密码调用码");
t3verify_set_code(&t3, "usercancel",   "注销账号调用码");
t3verify_set_code(&t3, "recharge",     "用户充值调用码");
t3verify_set_code(&t3, "kamirecharge", "卡密充值调用码");
t3verify_set_code(&t3, "unbind",       "卡密解绑调用码");
t3verify_set_code(&t3, "ipunbind",     "IP解绑调用码");
t3verify_set_code(&t3, "disable",      "禁用卡密调用码");
t3verify_set_code(&t3, "checkupdate",  "检查更新调用码");
t3verify_set_code(&t3, "getvariable",  "获取变量调用码");
t3verify_set_code(&t3, "modifyvar",    "修改变量调用码");
t3verify_set_code(&t3, "modifycore",   "修改核心数据调用码");
t3verify_set_code(&t3, "getcore",      "获取核心数据调用码");
t3verify_set_code(&t3, "getusercore",  "获取用户核心数据调用码");
t3verify_set_code(&t3, "onlinekami",   "在线卡密数调用码");
t3verify_set_code(&t3, "onlineuser",   "在线用户数调用码");
t3verify_set_code(&t3, "clouddoc",     "云文档调用码");
t3verify_set_code(&t3, "appsign",      "程序签名调用码");
t3verify_set_code(&t3, "qqheart",      "QQ心跳调用码");
t3verify_set_code(&t3, "getvarbyqq",   "按QQ取变量调用码");
t3verify_set_code(&t3, "getcorebyqq",  "按QQ取核心调用码");
t3verify_set_code(&t3, "modifycorebyqq", "按QQ改核心调用码");
t3verify_set_code(&t3, "unbindqq",     "解绑QQ调用码");
t3verify_set_code(&t3, "heartany",     "统一心跳调用码");
```

> 字段名以 `t3sdk.c` 中 `t3verify_set_code` 支持的名称为准；已设置过的代码可用新值覆盖。

## 4. 功能调用（API 清单）

所有函数返回值含义：**`0` = 成功**（此时看 `result->success`），**非 0** = 本地错误（网络失败/未初始化等）。`result->success` 为 1 表示服务端校验通过，0 表示业务失败，`result->error` 给出原因。

### 4.1 工具函数

```c
char machine[64];
get_machine_code(machine);                    /* 获取机器码（设备唯一标识） */
char md5[33];
md5_string_upper("hello", md5);               /* 计算字符串 MD5（大写16进制） */
```

### 4.2 卡密体系（单码）

```c
T3LoginResult login;
int r = t3verify_login(&t3, "卡密", "机器码", &login);
/* r==0 且 login.success==1 登录成功，字段： */
/* login.statecode  → 状态码（心跳要用） */
/* login.end_time   → 到期时间 */
/* login.imei       → 绑定的机器码 */
/* login.core       → 卡密核心数据 */
/* login.id/recharge/use_time/amount/available/change → 其他信息 */

T3QueryResult q;
t3verify_query_kami(&t3, "卡密", &q);          /* 查询卡密信息 */

T3Result hb;
t3verify_heartbeat(&t3, "卡密", login.statecode, &hb);   /* 心跳（登录后周期性调用） */

T3Result kr;
t3verify_kami_recharge(&t3, "目标卡密", "来源卡密", &kr);  /* 卡密充值 */
```

### 4.3 用户体系

```c
T3Result reg;
t3verify_user_register(&t3, "用户名", "密码", "邮箱(可空)", &reg);

T3LoginResult ul;
t3verify_user_login(&t3, "用户名", "密码", "机器码", &ul);

T3Result uh;
t3verify_user_heartbeat(&t3, "用户名", "密码", ul.statecode, &uh);

T3Result cp;
t3verify_change_password(&t3, "用户名", "旧密码", "新密码", &cp);

T3Result uc;
t3verify_user_cancel(&t3, "用户名", "密码", &uc);      /* 注销账号 */

T3Result rc;
t3verify_recharge(&t3, "用户名", "充值卡密", &rc);     /* 用户充值 */

/* QQ 登录体系 */
T3LoginResult ql;
t3verify_qq_login(&t3, "openid", "access_token", &ql);

T3Result bq;
t3verify_bind_qq(&t3, "用户名", "密码", "openid", "access_token", &bq);

T3Result uq;
t3verify_unbind_qq(&t3, "用户名", "密码", &uq);

T3Result qh;
t3verify_qq_heartbeat(&t3, "openid", "access_token", ql.statecode, &qh);
```

### 4.4 设备与安全（解绑/禁用）

```c
T3Result unb;
t3verify_unbind_kami(&t3, "卡密", "机器码", &unb);      /* 卡密解绑 */
t3verify_unbind_user(&t3, "用户名", "密码", "机器码", &unb);  /* 用户解绑 */
t3verify_ip_unbind_kami(&t3, "卡密", &unb);            /* 卡密IP解绑 */
t3verify_ip_unbind_user(&t3, "用户名", "密码", &unb);   /* 用户IP解绑 */
t3verify_disable_kami(&t3, "卡密", &unb);              /* 禁用卡密 */
t3verify_disable_user(&t3, "用户名", "密码", &unb);     /* 禁用用户 */
```

### 4.5 数据与内容（公告/版本/更新/云文档/签名）

```c
T3NoticeResult n;
t3verify_get_notice(&t3, &n);               /* 程序公告 → n.notice */

T3VersionResult v;
t3verify_get_latest_version(&t3, &v);       /* 最新版本号 → v.version */

T3UpdateResult up;
t3verify_check_update(&t3, "当前版本号", &up);   /* 检查更新 */
/* up.has_update==1 有更新，up.ver 新版本号，up.uplog 更新日志，up.upurl 下载地址 */

T3CloudDocResult doc;
t3verify_get_cloud_doc(&t3, "token", &doc); /* 云文档 → doc.content */

T3AppSignResult sign;
t3verify_app_sign(&t3, "待签名字符串", &sign);  /* 程序签名验证 → sign.autograph / sign.time */
```

### 4.6 远程变量（服务器端动态配置）

```c
T3VariableResult var;
/* 按卡密读取变量：valueid=变量ID, valuename=变量名 */
t3verify_get_variable_by_kami(&t3, "卡密", "valueid", "valuename", &var);  /* → var.value */
t3verify_get_variable_by_user(&t3, "用户名", "密码", "valueid", "valuename", &var);
t3verify_get_variable_by_qq(&t3, "openid", "access_token", "valueid", "valuename", &var);

/* 修改变量 */
T3Result mv;
t3verify_modify_variable_by_kami(&t3, "卡密", "valueid", "新内容", &mv);
t3verify_modify_variable_by_user(&t3, "用户名", "密码", "valueid", "新内容", &mv);
```

### 4.7 核心数据（自定义加密存储字段）

```c
T3CoreResult core;
t3verify_get_core_by_kami(&t3, "卡密", &core);          /* → core.core */
t3verify_get_core_by_user(&t3, "用户名", "密码", &core);
t3verify_get_user_core_by_qq(&t3, "openid", "access_token", &core);

T3Result mc;
t3verify_modify_core_by_kami(&t3, "卡密", "新核心数据", &mc);
t3verify_modify_core_by_user(&t3, "用户名", "密码", "新核心数据", &mc);
t3verify_modify_user_core_by_qq(&t3, "openid", "access_token", "新核心数据", &mc);
```

### 4.8 在线数量 / 统一心跳

```c
T3OnlineResult oc;
t3verify_get_online_kami_count(&t3, &oc);   /* 在线卡密数 → oc.count */
t3verify_get_online_user_count(&t3, &oc);   /* 在线用户数 → oc.count */

T3Result hb;
t3verify_heartbeat_any(&t3, "statecode", &hb);  /* 统一心跳（不区分卡密/用户/QQ） */
```

## 5. 完整主流程示例（登录 → 心跳 → 解绑）

```c
#include "t3sdk.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    T3Verify t3;
    if (t3verify_init(&t3, "登录调用码","公告调用码","版本调用码","心跳调用码",
                      "APPKEY", "自定义Base64字符集") != 0) {
        printf("初始化失败\n");
        return -1;
    }
    /* 设置其余调用码（如需解绑功能） */
    t3verify_set_code(&t3, "unbind", "解绑调用码");

    char machine[64] = {0};
    get_machine_code(machine);
    printf("机器码: %s\n", machine);

    /* 1. 登录 */
    T3LoginResult login = {0};
    if (t3verify_login(&t3, "你的卡密", machine, &login) != 0 || !login.success) {
        printf("登录失败: %s\n", login.error[0] ? login.error : "网络错误");
        return -1;
    }
    printf("登录成功, 到期时间: %s, 状态码: %s\n", login.end_time, login.statecode);

    /* 2. 心跳（建议每 60 秒调用一次） */
    for (int i = 0; i < 3; i++) {
        T3Result hb = {0};
        if (t3verify_heartbeat(&t3, "你的卡密", login.statecode, &hb) == 0 && hb.success) {
            printf("心跳正常\n");
        } else {
            printf("心跳失败: %s\n", hb.error[0] ? hb.error : "网络错误");
        }
        sleep(60);
    }

    /* 3. 解绑（换设备时需要） */
    T3Result unb = {0};
    if (t3verify_unbind_kami(&t3, "你的卡密", machine, &unb) == 0 && unb.success) {
        printf("解绑成功\n");
    }
    return 0;
}
```

## 6. 返回值结构体字段说明

| 结构体 | success 为 1 时的可用字段 |
|---|---|
| `T3Result` | `msg` 提示消息；失败时 `error` |
| `T3LoginResult` | `id`、`end_time` 到期时间、`statecode` 状态码、`recharge`、`use_time`、`amount`、`available`、`imei`、`change`、`core` |
| `T3NoticeResult` | `notice` 公告内容 |
| `T3VersionResult` | `version` 最新版本号 |
| `T3QueryResult` | `state`、`use`、`id`、`use_time`、`end_time`、`line_time`、`line`、`amount`、`available` |
| `T3UpdateResult` | `has_update`(1有更新)、`ver`、`version`、`uplog`、`upurl`、`msg` |
| `T3VariableResult` | `value` 变量值 |
| `T3CloudDocResult` | `content` 文档内容 |
| `T3CoreResult` | `core` 核心数据 |
| `T3OnlineResult` | `count` 数量 |
| `T3AppSignResult` | `msg`、`autograph`、`time`(时间戳) |

## 7. 常见问题

1. **登录返回 `success=0`**：`error` 里有服务端原因（卡密错误/已过期/机器码不匹配等），按提示处理。
2. **函数返回非 0**：本地错误，通常是网络不通或未初始化（`t3verify_init` 失败）。
3. **机器码变了**：卡密绑定的是机器码，换设备需先在原设备解绑，或使用 IP 解绑。
4. **如何换服务器**：SDK 默认内置 6 台官方服务器并自动探测；如需自定义，直接修改 `t3sdk.c` 顶部 `T3_SERVER_URLS` 数组后重新编译。
5. **线程安全**：SDK 无内部全局锁，多线程使用请为每个线程创建独立的 `T3Verify` 实例。
