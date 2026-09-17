# -*- coding: utf-8 -*-
"""
t3sdk.py - T3 网络验证 Python 对接模块
=======================================
对接 libt3sdk.so 同一套接口（通过 t3cli 调用，输出 JSON 解析）。
用法：与 Java 版 T3Verify 对齐，内置调用码不变。

依赖: 先编译 t3cli（见 README / docs 的 Shell+Python 对接文档）:
    g++ -std=c++17 -O2 -o t3cli t3cli.cpp ../versions/official-cpp/android/jni/t3sdk/t3sdk.cpp \
        -I ../versions/official-cpp/android/jni/t3sdk

快速使用:
    import t3sdk
    t = t3sdk.T3Verify()
    r = t.login("你的卡密", t3sdk.get_machine_code())
    if r["success"]:
        statecode = r["statecode"]
        print("到期时间:", r["endTime"])
        hb = t.heartbeat("你的卡密", statecode)   # 每 30 秒调用一次
"""

import json
import os
import subprocess

T3CLI = os.environ.get("T3CLI", os.path.join(os.path.dirname(os.path.abspath(__file__)), "t3cli"))


def _run(args):
    """调用 t3cli，返回解析后的 dict。"""
    try:
        out = subprocess.run([T3CLI] + args, capture_output=True, text=True, timeout=60).stdout
    except FileNotFoundError:
        return {"success": False, "error": "找不到 t3cli，请先编译（见文档）"}
    except subprocess.TimeoutExpired:
        return {"success": False, "error": "调用超时"}
    try:
        return json.loads(out)
    except Exception:
        return {"success": False, "error": out.strip() or "t3cli 无输出"}


def get_machine_code():
    """获取机器码（等价 Java: T3Verify.getMachineCode()）"""
    r = _run(["machine"])
    return r.get("machine", "")


class T3Verify:
    """等价 Java 的 com.t3yanzheng.sdk.T3Verify（凭证内置在 t3cli 里）。"""

    # ---------- 卡密体系 ----------
    def login(self, kami, imei=None):
        return _run(["login", kami])

    def query_kami(self, kami):
        return _run(["query", kami])

    def heartbeat(self, kami, statecode):
        return _run(["heartbeat", kami, statecode])

    def kami_recharge(self, target, source):
        return _run(["kami_recharge", target, source])

    def unbind_kami(self, kami, imei):
        return _run(["unbind_kami", kami, imei])

    def ip_unbind_kami(self, kami):
        return _run(["ip_unbind_kami", kami])

    def disable_kami(self, kami):
        return _run(["disable_kami", kami])

    # ---------- 数据与内容 ----------
    def get_notice(self):
        return _run(["notice"])

    def get_latest_version(self):
        return _run(["version"])

    def check_update(self, ver):
        return _run(["update", ver])

    def get_cloud_doc(self, token):
        return _run(["clouddoc", token])

    def app_sign(self, autograph):
        return _run(["sign", autograph])

    # ---------- 用户体系 ----------
    def user_register(self, user, pwd, email=""):
        return _run(["register", user, pwd, email] if email else ["register", user, pwd])

    def user_login(self, user, pwd, imei=None):
        return _run(["userlogin", user, pwd])

    def user_heartbeat(self, user, pwd, statecode):
        return _run(["userheartbeat", user, pwd, statecode])

    def change_password(self, user, old, new):
        return _run(["change_password", user, old, new])

    def user_cancel(self, user, pwd):
        return _run(["user_cancel", user, pwd])

    def recharge(self, user, card):
        return _run(["recharge", user, card])

    def unbind_user(self, user, pwd, imei):
        return _run(["unbind_user", user, pwd, imei])

    def ip_unbind_user(self, user, pwd):
        return _run(["ip_unbind_user", user, pwd])

    def disable_user(self, user, pwd):
        return _run(["disable_user", user, pwd])

    # ---------- 远程变量 ----------
    def get_variable_by_kami(self, kami, valueid, valuename):
        return _run(["getvar_kami", kami, valueid, valuename])

    def get_variable_by_user(self, user, pwd, valueid, valuename):
        return _run(["getvar_user", user, pwd, valueid, valuename])

    def modify_variable_by_kami(self, kami, valueid, content):
        return _run(["modifyvar_kami", kami, valueid, content])

    def modify_variable_by_user(self, user, pwd, valueid, content):
        return _run(["modifyvar_user", user, pwd, valueid, content])

    # ---------- 核心数据 ----------
    def get_core_by_kami(self, kami):
        return _run(["getcore_kami", kami])

    def get_core_by_user(self, user, pwd):
        return _run(["getcore_user", user, pwd])

    def modify_core_by_kami(self, kami, core):
        return _run(["modifycore_kami", kami, core])

    def modify_core_by_user(self, user, pwd, core):
        return _run(["modifycore_user", user, pwd, core])

    # ---------- 在线数量 / 统一心跳 ----------
    def get_online_kami_count(self):
        return _run(["online_kami"])

    def get_online_user_count(self):
        return _run(["online_user"])

    def heartbeat_any(self, statecode):
        return _run(["heartbeat_any", statecode])


# ========== 便捷函数（等价 Java 的 T3Helper） ==========

def verify(kami):
    """验证卡密：返回 (success:bool, msg:str, statecode:str)"""
    t = T3Verify()
    if not kami or not kami.strip():
        return False, "请输入卡密", ""
    r = t.login(kami.strip())
    if r.get("success"):
        return True, "验证成功\n卡密ID: %s\n到期时间: %s\n核心数据: %s" % (
            r.get("id", ""), r.get("endTime", ""), r.get("core", "")), r.get("statecode", "")
    return False, "验证失败: %s" % r.get("error", ""), ""


def heartbeat(kami, statecode):
    """心跳：返回 bool（等价 Java T3Helper.heartbeat）"""
    return bool(T3Verify().heartbeat(kami, statecode).get("success"))


if __name__ == "__main__":
    import sys
    print("机器码:", get_machine_code())
    t = T3Verify()
    print("公告:", json.dumps(t.get_notice(), ensure_ascii=False))
    print("最新版本:", json.dumps(t.get_latest_version(), ensure_ascii=False))
