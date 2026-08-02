# -*- coding: utf-8 -*-
"""
test_rain.py —— 雨天退化场景多变验证（功能测试版，无第三方依赖，run_tests.py 直接跑）
与 sim_core_logic.py 的「场景8」一一对应：盲道丢失/眩光误偏/误判终止/连续降级/
弱网离线/湿滑 GAP/摄像头糊住。全部离线可跑（无需 pytest/requests）。
运行： pytest test_rain.py -v   或   python run_tests.py
"""
from sim_core_logic import (
    GuidanceFSM, OfflineFallback, obstacle_to_feedback,
    mk_obs, simulate_main_loop,
    apply_cloud_weather, get_weather_mode_sim, reset_sim_weather, weather_is_wet,
    BP_FOLLOWING, BP_OFFSET_LEFT, BP_END_DETECTED,
)


def test_rain_path_loss_no_false_terminate():
    """雨天盲道短暂丢失(detected 抖动) 不应误终止"""
    fsm = GuidanceFSM()
    seq = [(True, BP_FOLLOWING), (False, BP_FOLLOWING), (True, BP_FOLLOWING),
           (False, BP_FOLLOWING), (True, BP_FOLLOWING)]
    out = [fsm.update(d, s) for d, s in seq]
    assert out[-1] == BP_FOLLOWING


def test_rain_glare_offset_filtered():
    """雨天眩光误判偏移，单次 spurious OFFSET 被滤除"""
    fsm = GuidanceFSM()
    s = [(True, BP_FOLLOWING), (True, BP_FOLLOWING), (True, BP_OFFSET_LEFT),
         (True, BP_FOLLOWING), (True, BP_FOLLOWING)]
    out = [fsm.update(d, st) for d, st in s]
    assert out[-1] == BP_FOLLOWING


def test_rain_false_end_filtered():
    """雨天湿面眩光误判“盲道终止”，单次 spurious END 被滤除（安全关键）"""
    fsm = GuidanceFSM()
    s = [(True, BP_FOLLOWING), (True, BP_FOLLOWING), (True, BP_END_DETECTED),
         (True, BP_FOLLOWING), (True, BP_FOLLOWING)]
    out = [fsm.update(d, st) for d, st in s]
    assert out[-1] == BP_FOLLOWING


def test_rain_true_end_still_switches():
    """真实连续 2 帧 END 仍应切换（避免漏报终止）"""
    fsm = GuidanceFSM()
    s = [(True, BP_FOLLOWING), (True, BP_END_DETECTED), (True, BP_END_DETECTED)]
    out = [fsm.update(d, st) for d, st in s]
    assert out[-1] == BP_END_DETECTED


def test_rain_consecutive_degraded_ultrasonic():
    """雨天连续降级(degraded) -> 离线超声波兜底生效"""
    ob = OfflineFallback()
    fbs = [ob.step(30, 1000 + i * 200) for i in range(3)]
    assert fbs[-1]["vibration"] == 90 and fbs[-1]["buzzer"]


def test_rain_degraded_no_misleading_voice():
    """降级但前方无障碍时不应播误导语音"""
    ob = OfflineFallback()
    fb = ob.step(300, 1000)
    assert fb["voice"] == ""


def test_rain_weak_net_offline_fast_and_protect():
    """雨天+弱网：离线帧仍 10Hz 非阻塞，且超声波保护"""
    seq = [{"connected": True, "rssi": -80, "sim_delay_ms": 2500},
           {"connected": False}, {"connected": False},
           {"connected": True, "rssi": -80}]
    tim = simulate_main_loop(seq)
    assert tim[1] < 100
    ob = OfflineFallback()
    assert ob.step(30, 1500)["vibration"] == 90


def test_rain_wet_road_gap_current_no_warn():
    """设计 GAP：当前固定阈值下 3.5m 不预警"""
    assert obstacle_to_feedback(mk_obs(3.5))["vibration"] == 0


def test_rain_wet_road_gap_rain_profile_warns():
    """雨模（天气模式开）：3.5m 应预警（LV1）"""
    assert obstacle_to_feedback(mk_obs(3.5), weather=True)["vibration"] > 0


def test_rain_wet_road_1_2m_more_conservative():
    """雨模在 1.2m 比当前档更保守（dry LV2 -> rain LV3）"""
    dry = obstacle_to_feedback(mk_obs(1.2), weather=False)["vibration"]
    rain = obstacle_to_feedback(mk_obs(1.2), weather=True)["vibration"]
    assert rain > dry


def test_rain_camera_blinded_fsm_safe_and_ultrasonic():
    """摄像头被雨水糊住(全程 detected=False) -> FSM 不误判 + 超声波保护"""
    fsm = GuidanceFSM()
    out = [fsm.update(False, BP_FOLLOWING) for _ in range(5)]
    assert out[-1] == BP_FOLLOWING
    ob = OfflineFallback()
    assert ob.step(30, 1800)["vibration"] == 90


# ============ 云端天气字段 -> 设备自动变档（与 sim 场景9 对应） ============
def test_cloud_weather_rain_enables_wet_and_warns_at_3_5m():
    """云端 weather=rain -> 自动开湿滑档，3.5m 开始预警"""
    reset_sim_weather()
    wet, changed = apply_cloud_weather("rain")
    assert wet and changed
    fb = obstacle_to_feedback(mk_obs(3.5), weather=get_weather_mode_sim())
    assert fb["vibration"] > 0


def test_cloud_weather_clear_keeps_dry_and_no_warn_at_3_5m():
    """云端 weather=clear -> 维持正常档（已干燥则 clear 不误切换），3.5m 不预警"""
    reset_sim_weather()
    wet, changed = apply_cloud_weather("clear")
    assert (not wet) and (not changed)
    fb = obstacle_to_feedback(mk_obs(3.5), weather=get_weather_mode_sim())
    assert fb["vibration"] == 0


def test_cloud_weather_repeat_no_chatter():
    """相同天气重复下发不重复切换（变更去抖）"""
    reset_sim_weather()
    _, c1 = apply_cloud_weather("rain")
    _, c2 = apply_cloud_weather("rain")
    assert c1 and (not c2)


def test_cloud_weather_snow_fog_are_wet():
    """snow/fog 也视为湿滑 -> 自动开档"""
    reset_sim_weather()
    wet_s, _ = apply_cloud_weather("snow")
    wet_f, _ = apply_cloud_weather("fog")
    assert wet_s and wet_f


def test_cloud_weather_unknown_no_false_wet():
    """未知/非法天气不误开湿滑档（保持干燥，且不发生误切换）"""
    reset_sim_weather()
    wet, changed = apply_cloud_weather("unknown")
    assert (not wet) and (not changed)


def test_weather_is_wet_mapping():
    """weather_is_wet 镜像：rain/snow/fog 湿滑，clear/其它 不湿滑"""
    assert weather_is_wet("rain") and weather_is_wet("snow") and weather_is_wet("fog")
    assert (not weather_is_wet("clear")) and (not weather_is_wet("unknown"))
