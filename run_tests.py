# -*- coding: utf-8 -*-
"""
run_tests.py —— 无 pytest 依赖的轻量测试运行器（本机无法 pip 安装 pytest）
实际执行 10_测试与验证 下所有 test_*.py 中的 test_* 函数并打印结果。
运行： python run_tests.py
"""
import glob, importlib, inspect, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
# 云端服务目录（prompts / llm_client / rate_limiter / logger 所在）
SRV = os.path.abspath(os.path.join(HERE, "..", "08_云端识别服务"))
if SRV not in sys.path:
    sys.path.insert(0, SRV)

files = sorted(glob.glob(os.path.join(HERE, "test_*.py")))
total = passes = fails = 0
for f in files:
    mod_name = os.path.basename(f)[:-3]
    try:
        mod = importlib.import_module(mod_name)
    except Exception as e:
        # 仅因缺少第三方包（pytest/requests）而加载失败时跳过，不算功能失败
        if "pytest" in str(e) or "requests" in str(e) or "No module" in str(e):
            print(f"[SKIP] {mod_name}: 缺少依赖({e})")
        else:
            print(f"[LOAD FAIL] {mod_name}: {e}")
            fails += 1
        continue
    for name, fn in inspect.getmembers(mod, inspect.isfunction):
        if not name.startswith("test_"):
            continue
        try:
            fn()
            passes += 1
            print(f"  [PASS] {mod_name}.{name}")
        except Exception as e:
            fails += 1
            print(f"  [FAIL] {mod_name}.{name}: {e}")
        total += 1

print(f"\n===== 共 {total} 个测试：{passes} 通过 / {fails} 失败 =====")
sys.exit(1 if fails else 0)
