# T3 网络验证 - Shell 与 Python 对接文档

> 适用版本：v3.1.4.1（回退自 v3.1.4）
> 与 libt3sdk.so 使用**同一套 C++ 接口、同一套内置调用码**，只换语言不换凭证。
> 代码在仓库 `tools/`：`t3cli.cpp`（C++ 命令行工具）、`t3sdk.py`（Python 模块）。

## 0. 原理

Android 的 `libt3sdk.so` 是 JNI 库，只能在 Android 里用。Linux/服务器上要对接同一套 T3 验证接口，做法是：

```
t3cli.cpp（调用与 .so 相同的 T3Verify C++ 类，内置相同调用码）
        │ 编译
        ▼
      t3cli（可执行文件，输出 JSON）
        │
        ├── Shell: 直接命令行调用
        └── Python: t3sdk.py 封装成类（等价 Java 的 T3Verify / T3Helper）
```

凭证（调用码/APPKEY/RSA 公钥）编译在 `t3cli` 里，与 `libt3sdk.so` 完全一致，**不要改动**。

## 1. 编译 t3cli

```bash
cd tools
g++ -std=c++17 -O2 -o t3cli t3cli.cpp ../versions/official-cpp/android/jni/t3sdk/t3sdk.cpp \
    -I ../versions/official-cpp/android/jni/t3sdk
# 验证
./t3cli machine        # 输出 {"machine":"你的机器码"}
```

> 只依赖 g++（无需 OpenSSL，SDK 自带 RSA/MD5/Base64 实现）。

## 2. Shell 对接

### 2.1 基础用法（输出 JSON）

```bash
./t3cli machine                               # 机器码
./t3cli login 813B2676E9690C89                # 卡密登录（第二参数机器码自动获取）
./t3cli notice                                # 公告
./t3cli version                               # 最新版本号
./t3cli query 你的卡密                          # 查询卡密
./t3cli online_kami                           # 在线卡密数
```

### 2.2 用 jq 取字段（推荐）

```bash
MACHINE=$(./t3cli machine | jq -r .machine)
LOGIN=$(./t3cli login "你的卡密")
echo "$LOGIN" | jq -r '"登录成功: \(.success)  到期: \(.endTime)  statecode: \(.statecode)"'

# 心跳（登录成功后每 30 秒一次）
STATECODE=$(echo "$LOGIN" | jq -r .statecode)
while true; do
  if ./t3cli heartbeat "你的卡密" "$STATECODE" | jq -e .success >/dev/null; then
    echo "[$(date +%T)] 心跳正常"
  else
    echo "[$(date +%T)] 心跳异常"
  fi
  sleep 30
done
```

### 2.3 不用 jq 的纯 Shell 解析（sed 取字段）

```bash
LOGIN=$(./t3cli login "你的卡密")
STATE=$(echo "$LOGIN" | sed -n 's/.*"success":\([a-z]*\).*/\1/p')
STATECODE=$(echo "$LOGIN" | sed -n 's/.*"statecode":"\([^"]*\)".*/\1/p')
[ "$STATE" = "true" ] && echo "登录成功, statecode=$STATECODE"
```

### 2.4 完整流程脚本示例（登录 → 公告 → 更新 → 心跳 → 解绑）

```bash
#!/usr/bin/env bash
set -euo pipefail
T3=./t3cli

KAMI="${1:-你的卡密}"

# 1. 登录（机器码自动获取）
LOGIN=$($T3 login "$KAMI")
if ! echo "$LOGIN" | jq -e .success >/dev/null; then
  echo "登录失败: $(echo "$LOGIN" | jq -r .error)"; exit 1
fi
STATECODE=$(echo "$LOGIN" | jq -r .statecode)
echo "登录成功, 到期时间: $(echo "$LOGIN" | jq -r .endTime)"

# 2. 公告 / 版本 / 更新
echo "公告: $($T3 notice | jq -r .notice)"
VER=$($T3 version | jq -r .version)
echo "最新版本: $VER"
$T3 update "$VER" | jq -r '"有更新: \(.hasUpdate)  新版本: \(.ver)  日志: \(.uplog)"'

# 3. 心跳 3 次（每次间隔 30 秒）
for i in 1 2 3; do
  if $T3 heartbeat "$KAMI" "$STATECODE" | jq -e .success >/dev/null; then
    echo "[$i] 心跳正常"; else echo "[$i] 心跳异常"; fi
  [ "$i" -lt 3 ] && sleep 30
done

# 4. 解绑（换设备时才需要）
$T3 unbind_kami "$KAMI" "$MACHINE" | jq -r '"解绑: \(.success) \(.error)"'
```

## 3. Python 对接

### 3.1 导入与基础调用（等价 Java 的 T3Verify）

```python
import t3sdk

t = t3sdk.T3Verify()
machine = t3sdk.get_machine_code()

# 登录
r = t.login("你的卡密")                 # 等价 Java: t3.login(kami, machine)
if r["success"]:
    print("到期时间:", r["endTime"])
    print("statecode:", r["statecode"])
    print("核心数据:", r["core"])
else:
    print("登录失败:", r["error"])

# 心跳（登录成功后每 30 秒调用一次）
hb = t.heartbeat("你的卡密", r["statecode"])
print("心跳:", "正常" if hb["success"] else "异常")
```

### 3.2 公告 / 版本 / 更新 / 解绑

```python
print("公告:", t.get_notice()["notice"])

ver = t.get_latest_version()["version"]
up = t.check_update(ver)
if up["success"] and up["hasUpdate"]:
    print("发现新版本", up["ver"], "更新日志:", up["uplog"])

unb = t.unbind_kami("你的卡密", machine)   # 换设备前解绑
print("解绑:", "成功" if unb["success"] else unb["error"])
```

### 3.3 便捷函数（等价 Java 的 T3Helper）

```python
ok, msg, statecode = t3sdk.verify("你的卡密")   # 验证卡密
print(msg)
alive = t3sdk.heartbeat("你的卡密", statecode)  # 心跳 -> bool
```

### 3.4 完整流程（登录 → 心跳循环 → 解绑）

```python
import time
import t3sdk

KAMI = "你的卡密"
t = t3sdk.T3Verify()

r = t.login(KAMI)
if not r["success"]:
    raise SystemExit("登录失败: " + r["error"])

print("登录成功, 到期:", r["endTime"], "statecode:", r["statecode"])

# 心跳循环（每 30 秒一次，返回 false 说明在线状态失效）
while True:
    if t.heartbeat(KAMI, r["statecode"]).get("success"):
        print(time.strftime("[%H:%M:%S] 心跳正常"))
    else:
        print(time.strftime("[%H:%M:%S] 心跳异常"))
        break
    time.sleep(30)
```

### 3.5 自动化发卡/批量查询示例

```python
import t3sdk

t = t3sdk.T3Verify()
for kami in ["卡密1", "卡密2", "卡密3"]:
    q = t.query_kami(kami)
    if q["success"]:
        print(f"{kami}: 状态={q['state']} 到期={q['endTime']} 可用={q['available']}")
    else:
        print(f"{kami}: 查询失败 {q['error']}")
```

## 4. 命令对照表（t3cli ↔ Java T3Verify）

| t3cli 命令 | 等价 Java 方法 | 说明 |
|---|---|---|
| `machine` | `T3Verify.getMachineCode()` | 机器码 |
| `login <卡密>` | `login(kami, imei)` | 卡密登录，机器码自动获取 |
| `query <卡密>` | `queryKami(kami)` | 查询卡密 |
| `heartbeat <卡密> <statecode>` | `heartbeat(kami, statecode)` | 心跳 |
| `notice` | `getNotice()` | 公告 |
| `version` | `getLatestVersion()` | 最新版本号 |
| `update <当前版本>` | `checkUpdate(ver)` | 检查更新 |
| `clouddoc <token>` | `getCloudDoc(token)` | 云文档 |
| `sign <字符串>` | `appSign(autograph)` | 程序签名 |
| `register <用户> <密码> [邮箱]` | `userRegister(...)` | 用户注册 |
| `userlogin <用户> <密码>` | `userLogin(...)` | 用户登录 |
| `userheartbeat <用户> <密码> <statecode>` | `userHeartbeat(...)` | 用户心跳 |
| `change_password <用户> <旧> <新>` | `changePassword(...)` | 修改密码 |
| `user_cancel <用户> <密码>` | `userCancel(...)` | 注销账号 |
| `recharge <用户> <卡密>` | `recharge(...)` | 用户充值 |
| `kami_recharge <目标> <来源>` | `kamiRecharge(...)` | 卡密充值 |
| `unbind_kami <卡密> <机器码>` | `unbindKami(...)` | 卡密解绑 |
| `unbind_user <用户> <密码> <机器码>` | `unbindUser(...)` | 用户解绑 |
| `ip_unbind_kami <卡密>` | `ipUnbindKami(...)` | IP解绑卡密 |
| `ip_unbind_user <用户> <密码>` | `ipUnbindUser(...)` | IP解绑用户 |
| `disable_kami <卡密>` | `disableKami(...)` | 禁用卡密 |
| `disable_user <用户> <密码>` | `disableUser(...)` | 禁用用户 |
| `getvar_kami <卡密> <ID> <名>` | `getVariableByKami(...)` | 按卡密读变量 |
| `getvar_user <用户> <密码> <ID> <名>` | `getVariableByUser(...)` | 按用户读变量 |
| `modifyvar_kami <卡密> <ID> <内容>` | `modifyVariableByKami(...)` | 改变量（卡密） |
| `modifyvar_user <用户> <密码> <ID> <内容>` | `modifyVariableByUser(...)` | 改变量（用户） |
| `getcore_kami <卡密>` | `getCoreByKami(...)` | 读核心（卡密） |
| `getcore_user <用户> <密码>` | `getCoreByUser(...)` | 读核心（用户） |
| `modifycore_kami <卡密> <内容>` | `modifyCoreByKami(...)` | 改核心（卡密） |
| `modifycore_user <用户> <密码> <内容>` | `modifyCoreByUser(...)` | 改核心（用户） |
| `online_kami` / `online_user` | `getOnlineKamiCount()` 等 | 在线数量 |
| `heartbeat_any <statecode>` | `heartbeatAny(...)` | 统一心跳 |

## 5. 输出 JSON 字段（与 Java 结果类一致）

| 命令 | success=true 时可用的字段 |
|---|---|
| `login` / `userlogin` | `id`、`endTime`、`statecode`、`recharge`、`useTime`、`amount`、`available`、`imei`、`change`、`core` |
| `query` | `state`、`use`、`id`、`useTime`、`endTime`、`lineTime`、`line`、`amount`、`available` |
| `notice` | `notice` |
| `version` | `version` |
| `update` | `hasUpdate`、`ver`、`version`、`uplog`、`upurl`、`msg` |
| `clouddoc` | `content` |
| `sign` | `msg`、`autograph`、`time` |
| `getvar_*` | `value` |
| `getcore_*` | `core` |
| `online_*` | `count` |
| 其余 | `msg`（失败时看 `error`） |

> `success=false` 时原因在 `error`；`error` 为空可能是网络不通。

## 6. 常见问题

1. **找不到 t3cli**：先按第 1 节编译；Python 可用环境变量 `T3CLI=/path/to/t3cli` 指定位置。
2. **`无法连接到所有T3网络验证服务器`**：检查服务器网络（`curl -I https://w.t3yanzheng.com/`）。若服务器需走代理，`t3cli` 是原生 socket 直连、不支持 HTTP 代理，请放到能直连的机器上。
3. **login 不需要手动传机器码**：`t3cli login` 自动获取（等价 Java 的 `login(kami, getMachineCode())`）。
4. **心跳多久一次**：登录成功后每 30 秒调用一次（与 APK 实际行为一致）。
5. **换设备**：先 `unbind_kami <卡密> <机器码>` 解绑，再在新设备登录。
6. **凭证**：调用码/APPKEY/RSA 公钥已内置在 `t3cli.cpp` 顶部宏中，与 `libt3sdk.so` 一致，不要改动；换自己的后台时改宏重新编译。
