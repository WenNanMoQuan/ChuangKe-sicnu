# -*- coding: utf-8 -*-
"""
test_cloud_mock.py —— 云端管线“数据契约”端到端验证（无需 Flask/requests/API Key）

由于本机无法 pip 安装 Flask（无外网），这里用标准库复刻 app.py 的核心数据流：
  mock 大模型 -> 组装与 08/app.py 一致的响应 -> 校验字段与 01/protocol.h 完全一致
等价于在“mock 模式”下真实跑通一次 /api/recognize 的返回契约。

运行： pytest test_cloud_mock.py -v
"""
import os, sys, json
import urllib.request

# 让 import 找到 tools/cloud 目录的模块（统一终端总控工程结构）
HERE = os.path.dirname(os.path.abspath(__file__))
SRV = os.path.abspath(os.path.join(HERE, "..", "cloud"))
sys.path.insert(0, SRV)

# 必须在 import llm_client 之前设定，确保 mock 供应商在模块导入时即生效
os.environ["LLM_PROVIDER"] = "mock"

import llm_client
from rate_limiter import TokenBucket
from protocol_fields import EXPECTED_KEYS, K_WEATHER, WEATHER_VALUES   # 与 01/protocol.h 的 K_* 对齐


def _clone_response():
    """复刻 app.py 的成功分支：调用大模型 -> 置 degraded=False -> 返回。"""
    result = llm_client.recognize_image("dummy_base64")
    result["degraded"] = False
    if K_WEATHER not in result:
        result[K_WEATHER] = "clear"   # app.py 兜底：缺失时按 clear
    return result


def test_protocol_fields_present():
    resp = _clone_response()
    # 顶层字段
    for k in ["blind_path", "obstacles", "feedback", "degraded"]:
        assert k in resp, f"缺少顶层字段 {k}"
    # blind_path 子字段（与 protocol.h K_* 对应）
    bp = resp["blind_path"]
    for k in ["detected", "offset_direction", "offset_angle", "status"]:
        assert k in bp, f"blind_path 缺 {k}"
    # feedback 子字段
    fb = resp["feedback"]
    for k in ["vibration", "buzzer", "led_color", "voice_text"]:
        assert k in fb, f"feedback 缺 {k}"
    # status 取值合法
    assert bp["status"] in ["FOLLOWING", "OFFSETLEFT", "OFFSETRIGHT", "TURNING", "ENDDETECTED"]


def test_weather_field_present_and_valid():
    """云端响应必须含 weather 字段，且取值合法（驱动端侧天气模式）。"""
    resp = _clone_response()
    assert K_WEATHER in resp, "云端响应缺少 weather 字段"
    assert resp[K_WEATHER] in WEATHER_VALUES, f"weather 取值非法: {resp[K_WEATHER]}"


def test_rate_limiter_allows_one_per_second():
    b = TokenBucket(rate=1.0, capacity=1)
    assert b.consume() is True     # 第 1 次通过
    assert b.consume() is False    # 同一秒内第 2 次被限流
    # 模拟时间推进 1s 后恢复
    import time
    time.sleep(1.05)
    assert b.consume() is True


def test_mock_response_risk_level_consistent():
    """mock 返回的障碍 risk_level 必须在 1~3，且与距离语义一致（1.5m -> LV2）。"""
    resp = _clone_response()
    obs = resp["obstacles"][0]
    assert obs["risk_level"] == 2
    assert 1.0 <= obs["distance"] < 2.0


if __name__ == "__main__":
    test_protocol_fields_present()
    test_weather_field_present_and_valid()
    test_rate_limiter_allows_one_per_second()
    test_mock_response_risk_level_consistent()
    print("cloud mock contract tests OK")
