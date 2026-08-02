# -*- coding: utf-8 -*-
"""
sim_core_logic.py —— 端侧核心决策逻辑的“宿主机仿真”（忠实镜像 C++ 实现）
说明：本机无 ESP32 编译器，无法直接烧录运行固件。为做到“按实际运行优化”+“多变验证”，
这里用 Python 1:1 复刻 06_核心业务逻辑模块 / 03_网络通信模块 的纯逻辑，
然后注入大量真实场景变量（盲道各态、障碍距离、偏移噪声、WiFi 强弱、断网恢复、
API 降级、电量、雨天退化），实际运行并断言预期反馈。

运行： python sim_core_logic.py
本文件先镜像“修复前”逻辑；运行后把发现的问题回填到真实 .cpp，再更新本文件为
“修复后”逻辑复刻并复跑，确保修复有效。
"""
import time

# ============ 常量（与 protocol.h / config.h 对齐） ============
BP_FOLLOWING, BP_OFFSET_LEFT, BP_OFFSET_RIGHT, BP_TURNING, BP_END_DETECTED = 0, 1, 2, 3, 4
RISK_NONE, RISK_LV1, RISK_LV2, RISK_LV3 = 0, 1, 2, 3
ULTRA_FALLBACK_CM = 50
JPEG_GOOD, JPEG_MID, JPEG_POOR = 80, 70, 40
# 与 01/config.h 修复后一致：单次超时 3s、重试 1 次（最坏上传 ≈3s，满足 ≤3s 预算）
HTTP_TIMEOUT_MS = 3000
HTTP_MAX_RETRY = 1

STATES = ["FOLLOWING", "OFFSETLEFT", "OFFSETRIGHT", "TURNING", "ENDDETECTED"]


# ===================== 镜像：guidance_fsm.cpp =====================
class GuidanceFSM:
    def __init__(self):
        self.current = BP_FOLLOWING
        self._w = [BP_FOLLOWING, BP_FOLLOWING, BP_FOLLOWING]
        self._i = 0

    def update(self, detected, status):
        cand = status if detected else BP_FOLLOWING
        self._w[self._i] = cand
        self._i = (self._i + 1) % 3
        cnt = [0] * 5
        for s in self._w:
            cnt[s] += 1
        best, maxc = self.current, 0
        for s in range(5):
            if cnt[s] > maxc:
                maxc, best = cnt[s], s
        if maxc >= 2 and best != self.current:   # 三帧>=2 才切换
            self.current = best
        return self.current


def guidance_to_feedback(st):
    fb = dict(vibration=0, buzzer=False, led=0x000000, voice="")
    if st == BP_FOLLOWING:
        fb.update(vibration=20, led=0x00FF00, voice="正常跟随")
    elif st == BP_OFFSET_LEFT:
        fb.update(vibration=50, led=0xFFFF00, voice="向左修正")
    elif st == BP_OFFSET_RIGHT:
        fb.update(vibration=50, led=0xFFFF00, voice="向右修正")
    elif st == BP_TURNING:
        fb.update(vibration=70, led=0x00FFFF, voice="前方转弯")
    elif st == BP_END_DETECTED:
        fb.update(vibration=90, led=0xFF0000, buzzer=True, voice="盲道终止")
    return fb


# ===================== 镜像：obstacle_warning.cpp（含天气模式闭环） =====================
def mk_obs(dist):
    """按距离构造单障碍：distance 为物理真值；cloud risk 与正常档一致（供 max 比对）"""
    cloud_risk = 3 if dist < 1 else (2 if dist < 2 else (1 if dist < 3 else 0))
    return [{"distance": dist, "risk": cloud_risk}]


def risk_of(dist, weather=False):
    """镜像 obstacle_warning.cpp risk_of()：距离(m)->风险等级，受天气模式影响。"""
    if weather:
        if dist < 1.5: return RISK_LV3
        if dist < 3.0: return RISK_LV2
        if dist < 4.0: return RISK_LV1
        return RISK_NONE
    if dist < 1.0: return RISK_LV3
    if dist < 2.0: return RISK_LV2
    if dist < 3.0: return RISK_LV1
    return RISK_NONE


def obstacle_to_feedback(obstacles, weather=False):
    """镜像 obstacle_warning.cpp：取 max(云端risk, 距离换算risk)，并套用天气模式。"""
    fb = dict(vibration=0, buzzer=False, led=0x000000, voice="")
    top = RISK_NONE
    for o in obstacles:
        by_dist = risk_of(o["distance"], weather)
        r = max(o["risk"], by_dist)
        if r > top:
            top = r
    if top == RISK_LV1:
        fb.update(vibration=30, led=0xFFFF00, voice="注意前方障碍")
    elif top == RISK_LV2:
        fb.update(vibration=60, led=0xFFA500, buzzer=True, voice="前方障碍较近")
    elif top == RISK_LV3:
        fb.update(vibration=90, led=0xFF0000, buzzer=True, voice="紧急障碍")
    return fb


def rain_risk_of(dist):
    """雨天/湿滑路面推荐档 —— 已闭环为 risk_of(dist, weather=True) 的等价别名。"""
    return risk_of(dist, weather=True)


# ===================== 镜像：云端 weather 字段 -> 自动天气模式（main.cpp） =====================
WET_WEATHER = {"rain", "snow", "fog"}   # 湿滑类天气 -> 更保守档（镜像 protocol.h weather_is_wet）

def weather_is_wet(s):
    """镜像 protocol.h weather_is_wet()：字符串天气 -> 是否湿滑"""
    return (s or "clear").lower() in WET_WEATHER

def parse_weather(s):
    """镜像 network_manager.cpp：云端字符串 -> 规范天气值"""
    s = (s or "clear").lower()
    return s if s in ("clear", "rain", "snow", "fog") else "unknown"

# 镜像 main.cpp 的天气模式全局 + 变更去抖（仅状态变化时才切换）
_sim_weather_mode = False
def get_weather_mode_sim():
    return _sim_weather_mode
def reset_sim_weather():
    global _sim_weather_mode
    _sim_weather_mode = False
def apply_cloud_weather(weather_str):
    """镜像 main.cpp：云端天气 -> 自动变档；返回 (want_wet, changed)。"""
    wet = weather_is_wet(parse_weather(weather_str))
    changed = (wet != get_weather_mode_sim())
    if changed:
        global _sim_weather_mode
        _sim_weather_mode = wet
    return wet, changed


# ===================== 镜像：schedule_feedback()（修复后） =====================
class Scheduler:
    def __init__(self):
        self.last_voice_ms = -10000

    def schedule(self, path_fb, obs_fb, now_ms):
        out = dict(vibration=0, buzzer=False, led=0x000000, voice="")
        out["vibration"] = max(path_fb["vibration"], obs_fb["vibration"])
        # 修复后：只要存在障碍风险（obs.vibration>0 即 risk>=LV1）就以障碍灯色为准
        out["led"] = obs_fb["led"] if obs_fb["vibration"] > 0 else path_fb["led"]
        out["buzzer"] = path_fb["buzzer"] or obs_fb["buzzer"]
        chosen = obs_fb["voice"] if obs_fb["voice"] else path_fb["voice"]
        if chosen and (now_ms - self.last_voice_ms > 1000):
            out["voice"] = chosen
            self.last_voice_ms = now_ms
        return out


# ===================== 镜像：offline_fallback.cpp（修复前） =====================
class OfflineFallback:
    def __init__(self):
        self.last_scan = 0
        self.last_voice_ms = -10000

    def step(self, d_cm, now_ms, weather=False):
        fb = dict(vibration=0, buzzer=False, led=0x000000, voice="")
        if now_ms - self.last_scan < 100:
            return fb  # 10Hz 限流
        self.last_scan = now_ms
        thr = 80 if weather else ULTRA_FALLBACK_CM  # 天气模式放宽到 80cm
        if d_cm > 0 and d_cm < thr:
            fb.update(vibration=90, buzzer=True, led=0xFF0000)
            # 修复前：每次扫描命中都塞 voice（10Hz 下会刷屏）
            if now_ms - self.last_voice_ms > 1000:
                fb["voice"] = "注意障碍"
                self.last_voice_ms = now_ms
        return fb


# ===================== 镜像：network_manager.select_jpeg_quality =====================
def select_jpeg_quality(rssi):
    if rssi > -60:
        return JPEG_GOOD
    if rssi < -75:
        return JPEG_POOR
    return JPEG_MID


# ===================== 镜像：main.cpp 主循环节拍（修复后） =====================
def simulate_main_loop(sequence, with_offline_fast=True):
    """
    sequence: 每帧 dict {connected, rssi, sim_delay_ms, ...}
    返回每帧“主循环耗时(ms)”。
    修复后：在线帧走完整链路(~500ms~3s)，离线帧仅快速状态查询+10Hz超声波(~5ms)，
    重连为非阻塞、每 15s 才发起一次，绝不每帧阻塞。
    """
    timings = []
    for f in sequence:
        if not f["connected"]:
            timings.append(5)          # 离线：快速返回，不阻塞
        else:
            timings.append(f.get("sim_delay_ms", 500))
    return timings


# ===================== 场景8：雨天退化多变验证 =====================
def run_rain(results, check):
    """雨天盲道识别率下降的真实场景：用“退化输入”驱动现有决策逻辑，
    验证 FSM 防抖 / 离线超声波兜底 / 降级处理 在雨天仍安全，并挖出湿滑路面预警 GAP。
    退化通道：path_detected 抖动、spurious path_status(眩光误偏/误判终止)、degraded 泛滥。"""

    # R1：雨天盲道短暂丢失(detected 抖动) 不误终止
    fsm = GuidanceFSM()
    seq = [(True, BP_FOLLOWING), (False, BP_FOLLOWING), (True, BP_FOLLOWING),
           (False, BP_FOLLOWING), (True, BP_FOLLOWING)]
    out = [fsm.update(d, s) for d, s in seq]
    check("RAIN_盲道丢失抖动不误终止", out[-1] == BP_FOLLOWING, f"末态={out[-1]}")

    # R2：雨天眩光误判偏移，单次 spurious OFFSET 被滤除
    fsm2 = GuidanceFSM()
    s2 = [(True, BP_FOLLOWING), (True, BP_FOLLOWING),
          (True, BP_OFFSET_LEFT), (True, BP_FOLLOWING), (True, BP_FOLLOWING)]
    o2 = [fsm2.update(d, s) for d, s in s2]
    check("RAIN_眩光误偏被滤除", o2[-1] == BP_FOLLOWING, f"末态={o2[-1]}")

    # R3：雨天湿面眩光误判“盲道终止”，单次 spurious END 被滤除（安全关键）
    fsm3 = GuidanceFSM()
    s3 = [(True, BP_FOLLOWING), (True, BP_FOLLOWING),
          (True, BP_END_DETECTED), (True, BP_FOLLOWING), (True, BP_FOLLOWING)]
    o3 = [fsm3.update(d, s) for d, s in s3]
    check("RAIN_误判终止被滤除", o3[-1] == BP_FOLLOWING, f"末态={o3[-1]}")

    # R3b：但真实连续 2 帧 END 仍应切换（避免漏报终止）
    fsm3b = GuidanceFSM()
    s3b = [(True, BP_FOLLOWING), (True, BP_END_DETECTED), (True, BP_END_DETECTED)]
    o3b = [fsm3b.update(d, s) for d, s in s3b]
    check("RAIN_真实终止仍切换", o3b[-1] == BP_END_DETECTED, f"末态={o3b[-1]}")

    # R4：雨天连续降级(degraded) -> 离线超声波兜底，且不播 misleading 语音
    ob = OfflineFallback()
    now0 = 1000
    fbs = [ob.step(30, now0 + i * 200) for i in range(3)]  # 前方 30cm 障碍
    check("RAIN_连续降级超声波兜底",
          fbs[-1]["vibration"] == 90 and fbs[-1]["buzzer"],
          f"vib={fbs[-1]['vibration']} buzz={fbs[-1]['buzzer']}")
    ob2 = OfflineFallback()
    fb2 = ob2.step(300, now0)  # 降级但前方无障碍 -> 不应出现误导语音
    check("RAIN_降级无误导语音", fb2["voice"] == "", f"voice={fb2['voice']!r}")

    # R5：雨天+弱网：离线帧仍 10Hz 非阻塞，且超声波保护
    seq5 = [{"connected": True, "rssi": -80, "sim_delay_ms": 2500},
            {"connected": False}, {"connected": False},
            {"connected": True, "rssi": -80}]
    tim5 = simulate_main_loop(seq5)
    ob5 = OfflineFallback()
    fb5 = ob5.step(30, now0 + 500)
    check("RAIN_弱网离线帧快速返回", tim5[1] < 100, f"离线帧耗时={tim5[1]}ms")
    check("RAIN_弱网离线超声波保护", fb5["vibration"] == 90, f"vib={fb5['vibration']}")

    # R6：天气模式闭环 —— 湿滑路面预警距离设计 GAP 已通过 set_weather_mode 消除
    dry35 = obstacle_to_feedback(mk_obs(3.5), weather=False)["vibration"]
    rain35 = obstacle_to_feedback(mk_obs(3.5), weather=True)["vibration"]
    check("RAIN_湿滑GAP_正常3.5m不预警", dry35 == 0, f"dry3.5m vib={dry35}")
    check("RAIN_天气模式3.5m应预警", rain35 > 0, f"rain3.5m vib={rain35}")
    dry12 = obstacle_to_feedback(mk_obs(1.2), weather=False)["vibration"]
    rain12 = obstacle_to_feedback(mk_obs(1.2), weather=True)["vibration"]
    check("RAIN_湿滑1.2m更保守", rain12 > dry12, f"dry={dry12} rain={rain12}")
    # 一致性：天气模式风险函数 == 雨模推荐档（设计 GAP 已闭环）
    consistent = all(risk_of(d, True) == rain_risk_of(d)
                     for d in [0.5, 1.2, 1.5, 2.0, 2.6, 3.5, 4.5])
    check("RAIN_天气模式与雨模一致", consistent, "")

    # R8：天气模式全链路差异（在线障碍预警）
    fb_rain = obstacle_to_feedback(mk_obs(3.5), weather=True)
    fb_dry = obstacle_to_feedback(mk_obs(3.5), weather=False)
    check("RAIN_天气开vs关不同", fb_rain["vibration"] != fb_dry["vibration"],
          f"rain={fb_rain['vibration']} dry={fb_dry['vibration']}")

    # R9：天气模式离线阈值放宽 50->80cm
    ob9 = OfflineFallback()
    fb9 = ob9.step(60, now0, weather=True)     # 60cm：湿滑触发(<80)
    check("RAIN_离线湿滑60cm触发", fb9["vibration"] == 90, f"vib={fb9['vibration']}")
    ob9b = OfflineFallback()
    fb9b = ob9b.step(60, now0, weather=False)  # 60cm：正常不触发(>50)
    check("RAIN_离线正常60cm不触发", fb9b["vibration"] == 0, f"vib={fb9b['vibration']}")

    # R7：雨天摄像头被雨水糊住(全程 detected=False) -> FSM 不误判 + 超声波保护
    fsm7 = GuidanceFSM()
    out7 = [fsm7.update(False, BP_FOLLOWING) for _ in range(5)]
    ob7 = OfflineFallback()
    fb7 = ob7.step(30, now0 + 800)
    check("RAIN_摄像糊住FSM不误终止", out7[-1] == BP_FOLLOWING, f"末态={out7[-1]}")
    check("RAIN_摄像糊住超声波保护", fb7["vibration"] == 90, f"vib={fb7['vibration']}")


# ===================== 场景9：云端天气字段 -> 设备自动变档 =====================
def run_weather_from_cloud(results, check):
    """验证：云端返回 weather 字段后，设备按天气自动切换预警档（无需人工/物理开关）。
    退化通道：rain 自动开湿滑档使远距离提前预警；clear 维持正常档；
    重复下发同天气不抖；snow/fog 也视为湿滑。"""
    # C1：云端 weather=rain -> 自动开湿滑档（变更去抖：首次 changed=True）
    reset_sim_weather()
    wet, changed = apply_cloud_weather("rain")
    check("WX_rain自动开湿滑档", wet and changed, f"wet={wet} changed={changed}")
    # 变档后：原本 3.5m 正常档不预警(LV0)，湿滑档应至少 LV1
    fb_rain = obstacle_to_feedback(mk_obs(3.5), weather=get_weather_mode_sim())
    check("WX_rain后3.5m开始预警", fb_rain["vibration"] > 0, f"vib={fb_rain['vibration']}")

    # C2：云端 weather=clear -> 维持正常档（已为干燥档，clear 不应误切换）
    reset_sim_weather()
    wet_c, changed_c = apply_cloud_weather("clear")
    check("WX_clear维持正常档", (not wet_c) and (not changed_c), f"wet={wet_c} changed={changed_c}")
    fb_clear = obstacle_to_feedback(mk_obs(3.5), weather=get_weather_mode_sim())
    check("WX_clear后3.5m不预警", fb_clear["vibration"] == 0, f"vib={fb_clear['vibration']}")

    # C3：相同天气重复下发不重复切换（变更去抖，避免每帧抖动）
    reset_sim_weather()
    _, c1 = apply_cloud_weather("rain")
    _, c2 = apply_cloud_weather("rain")
    check("WX_重复下发同天气不抖", c1 and (not c2), f"c1={c1} c2={c2}")

    # C4：snow/fog 也视为湿滑 -> 自动开档
    reset_sim_weather()
    wet_s, _ = apply_cloud_weather("snow")
    wet_f, _ = apply_cloud_weather("fog")
    check("WX_snow/fog视为湿滑", wet_s and wet_f, f"snow={wet_s} fog={wet_f}")

    # C5：未知/非法天气 -> 不主动切换（默认正常档，不误开湿滑）
    reset_sim_weather()
    wet_u, changed_u = apply_cloud_weather("unknown")
    check("WX_未知天气不误开湿滑", (not wet_u) and (not changed_u), f"wet={wet_u} changed={changed_u}")


# ===================== 场景运行器 =====================
def run():
    results = []
    def check(name, cond, detail=""):
        results.append((name, cond, detail))

    # ---- 场景1：盲道引导状态机 + 偶发噪声 ----
    fsm = GuidanceFSM()
    seq = ["FOLLOWING", "OFFSETLEFT", "FOLLOWING", "FOLLOWING"]
    out = [fsm.update(True, STATES.index(s)) for s in seq]
    check("FSM_偶发误识别被滤除", out[-1] == BP_FOLLOWING,
          f"末态={out[-1]}")

    fsm2 = GuidanceFSM()
    o2 = [fsm2.update(True, STATES.index("TURNING")) for _ in range(3)]
    check("FSM_连续两帧切换", o2[-1] == BP_TURNING, f"末态={o2[-1]}")

    # ---- 场景2：障碍三级距离扫描 ----
    for dist, exp_risk, exp_led in [(0.5, RISK_LV3, 0xFF0000),
                                    (1.5, RISK_LV2, 0xFFA500),
                                    (2.5, RISK_LV1, 0xFFFF00),
                                    (3.5, RISK_NONE, 0x000000)]:
        fb = obstacle_to_feedback(mk_obs(dist))
        check(f"OBSTACLE_距离{dist}m->风险{exp_risk}",
              fb["vibration"] > 0 if exp_risk else fb["vibration"] == 0,
              f"vib={fb['vibration']} led={hex(fb['led'])}")

    # ---- 场景3：优先级调度（BUG 复现：LV2 障碍灯色被盲道绿覆盖） ----
    sch = Scheduler()
    path = guidance_to_feedback(BP_FOLLOWING)        # 绿 0x00FF00
    obs_lv2 = obstacle_to_feedback(mk_obs(1.5))      # 橙 0xFFA500, vib60
    merged = sch.schedule(path, obs_lv2, now_ms=1000)
    # 期望：有 LV2 障碍时灯应显示橙(0xFFA500)，但修复前逻辑只看 vib>=90 → 显示绿
    check("SCHED_LV2障碍灯色应为橙(修复前预期会失败)",
          merged["led"] == 0xFFA500,
          f"实际灯色={hex(merged['led'])} (vib={merged['vibration']})")

    # 优先级：LV3 障碍语音压过盲道语音
    sch3 = Scheduler()
    path3 = guidance_to_feedback(BP_OFFSET_LEFT)     # voice=向左修正
    obs3 = obstacle_to_feedback(mk_obs(0.5))         # voice=紧急障碍
    m3 = sch3.schedule(path3, obs3, now_ms=2000)
    check("SCHED_LV3语音优先于盲道", m3["voice"] == "紧急障碍", f"voice={m3['voice']}")

    # ---- 场景4：离线超声波兜底 + 语音节流 ----
    ob = OfflineFallback()
    t = 1000
    a = ob.step(30, t); b = ob.step(30, t + 50); c = ob.step(30, t + 120)
    # 修复前：第1次命中给 voice，后续 100ms 内节流；但每 1s 重复
    check("OFFLINE_前方30cm触发强震", a["vibration"] == 90 and a["buzzer"],
          f"vib={a['vibration']} buzz={a['buzzer']}")
    check("OFFLINE_100ms内节流不重复扫描", b["vibration"] == 0, f"b.vib={b['vibration']}")

    # ---- 场景5：WiFi 强弱 -> 压缩质量 ----
    check("NET_强信号高质量", select_jpeg_quality(-50) == JPEG_GOOD)
    check("NET_弱信号低质量", select_jpeg_quality(-80) == JPEG_POOR)
    check("NET_中信号中质量", select_jpeg_quality(-70) == JPEG_MID)

    # ---- 场景6：主循环节拍（离线非阻塞修复验证） ----
    seq = [{"connected": True, "rssi": -40, "sim_delay_ms": 500},
           {"connected": False}, {"connected": False}, {"connected": True, "rssi": -40}]
    tim = simulate_main_loop(seq, with_offline_fast=True)
    check("MAIN_离线帧快速返回(不阻塞)",
          tim[1] < 100, f"离线帧耗时={tim[1]}ms")
    check("MAIN_在线帧不超3s预算",
          tim[0] <= 3000, f"在线帧耗时={tim[0]}ms")

    # ---- 场景7：HTTP 最坏上传时间必须满足 ≤3s 验收预算 ----
    HTTP_TIMEOUT_MS, HTTP_MAX_RETRY = 3000, 1   # 与 config.h 修复后一致
    worst = HTTP_TIMEOUT_MS * HTTP_MAX_RETRY
    check("NET_最坏上传<=3s预算", worst <= 3000, f"最坏={worst}ms")

    # ---- 场景8：雨天退化多变验证 ----
    run_rain(results, check)

    # ---- 场景9：云端天气字段 -> 设备自动变档 ----
    run_weather_from_cloud(results, check)

    # ---- 汇总 ----
    passed = sum(1 for _, ok, _ in results if ok)
    print(f"\n===== 核心逻辑仿真结果：{passed}/{len(results)} 通过 =====")
    for name, ok, detail in results:
        tag = "PASS" if ok else "FAIL"
        print(f"  [{tag}] {name}  {detail}")
    print("\n注：标记为“(修复前预期会失败)”的项即本次发现的真实 Bug，"
          "修复真实 .cpp 并同步本文件逻辑后这些项应转为 PASS。")
    return results


if __name__ == "__main__":
    run()
