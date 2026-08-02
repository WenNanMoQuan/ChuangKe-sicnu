# -*- coding: utf-8 -*-
"""
test_simulation.py —— 多变场景验证（pytest 驱动 sim_core_logic 的纯逻辑模型）
覆盖：盲道各态+噪声、障碍距离扫描(0.5~3.5m)、优先级调度(LV2/LV3)、
离线超声波阈值、WiFi 强弱->压缩质量、最坏上传延迟<=3s。

运行： pytest test_simulation.py -v
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from sim_core_logic import (GuidanceFSM, obstacle_to_feedback, Scheduler,
                            OfflineFallback, select_jpeg_quality,
                            BP_FOLLOWING, BP_TURNING, BP_OFFSET_LEFT,
                            RISK_NONE, RISK_LV1, RISK_LV2, RISK_LV3,
                            STATES, JPEG_GOOD, JPEG_MID, JPEG_POOR,
                            HTTP_TIMEOUT_MS, HTTP_MAX_RETRY)


def _obs(dist):
    risk = 3 if dist < 1 else (2 if dist < 2 else (1 if dist < 3 else 0))
    return [{"distance": dist, "risk": risk}]


def test_fsm_ignores_single_glitch():
    f = GuidanceFSM()
    assert f.update(True, STATES.index("FOLLOWING")) == BP_FOLLOWING
    f.update(True, STATES.index("OFFSETLEFT"))
    f.update(True, STATES.index("FOLLOWING"))
    assert f.current == BP_FOLLOWING


def test_fsm_two_of_three_switches():
    f = GuidanceFSM()
    assert f.update(True, STATES.index("TURNING")) == BP_FOLLOWING
    assert f.update(True, STATES.index("TURNING")) == BP_TURNING


def test_obstacle_distance_sweep():
    expect = [(0.5, RISK_LV3), (1.5, RISK_LV2), (2.5, RISK_LV1), (3.5, RISK_NONE)]
    for dist, lvl in expect:
        fb = obstacle_to_feedback(_obs(dist))
        top = 3 if fb["vibration"] == 90 else (2 if fb["vibration"] == 60
              else (1 if fb["vibration"] == 30 else 0))
        assert top == lvl, f"dist={dist} 期望风险{lvl} 实际{top}"


def test_schedule_lv2_led_is_orange():
    # 修复后：LV2 障碍灯色应保留橙色，不被盲道绿色覆盖
    sch = Scheduler()
    path = {"vibration": 20, "buzzer": False, "led": 0x00FF00, "voice": "正常跟随"}
    obs = obstacle_to_feedback(_obs(1.5))   # 橙 0xFFA500, vib60
    merged = sch.schedule(path, obs, now_ms=1000)
    assert merged["led"] == 0xFFA500


def test_schedule_lv3_voice_priority():
    sch = Scheduler()
    path = {"vibration": 50, "buzzer": False, "led": 0xFFFF00, "voice": "向左修正"}
    obs = obstacle_to_feedback(_obs(0.5))
    m = sch.schedule(path, obs, now_ms=2000)
    assert m["voice"] == "紧急障碍"


def test_offline_ultrasonic_threshold():
    ob = OfflineFallback()
    hit = ob.step(30, 1000)
    assert hit["vibration"] == 90 and hit["buzzer"] is True
    miss = ob.step(80, 1200)   # 80cm 超过 50cm 阈值
    assert miss["vibration"] == 0


def test_offline_voice_throttled_to_1s():
    ob = OfflineFallback()
    a = ob.step(30, 1000)
    b = ob.step(30, 1100)   # 100ms 后，语音应被节流
    assert a["voice"] == "注意障碍"
    assert b["voice"] == ""


def test_jpeg_quality_by_rssi():
    assert select_jpeg_quality(-50) == JPEG_GOOD
    assert select_jpeg_quality(-80) == JPEG_POOR
    assert select_jpeg_quality(-70) == JPEG_MID


def test_worst_upload_within_3s_budget():
    assert HTTP_TIMEOUT_MS * HTTP_MAX_RETRY <= 3000
