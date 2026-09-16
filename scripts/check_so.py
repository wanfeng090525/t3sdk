#!/usr/bin/env python3
"""校验 Android .so 产物包含 chunked/content-length 修复代码。

用法: python3 check_so.py <libs目录>
"""
import os
import sys

LIB_DIR = sys.argv[1]
ARCHES = ["arm64-v8a", "armeabi-v7a", "x86", "x86_64"]
CHECKS = [
    (b"transfer-encoding: chunked", "缺失 chunked 检测"),
    (b"content-length:", "缺失 content-length 解析"),
    (b"0\r\n\r\n", "缺失 dechunk 结束标记"),
]

ok = True
for arch in ARCHES:
    path = os.path.join(LIB_DIR, arch, "libt3sdk.so")
    if not os.path.exists(path):
        print(f"FAIL {arch}: 产物缺失 {path}")
        ok = False
        continue
    data = open(path, "rb").read()
    for needle, msg in CHECKS:
        if needle not in data:
            print(f"FAIL {arch}: {msg}")
            ok = False
        else:
            print(f"OK   {arch}: {needle!r}")

sys.exit(0 if ok else 1)
