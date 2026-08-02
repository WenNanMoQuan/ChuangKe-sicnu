#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
sim_ondevice_decode.py —— 端侧 AI 解码 + 离线闭环决策 等价仿真
================================================================
本机无 ESP32 编译器 / 无 GPU，无法真跑 TFLite Micro，故用 Python 复刻
ondevice_detect.cpp 的「解码 + 业务映射」与 main.cpp 的「离线闭环决策」，
验证关键逻辑正确性（盲道偏移、斑马线/井盖、障碍距离+风险、端侧主+超声底线）。

离线闭环（阶段八收口）：设备脱离网络，识别主路径=端侧 TFLite Micro；
安全底线=HC-SR04 超声波（融合 + 无模型兜底）。无云端分支。

运行： python sim_ondevice_decode.py
"""
import math

# ---------- 复刻 config.h 关键常量 ----------
LOCAL_DETECT_W, LOCAL_DETECT_H = 96, 96
CLASS_BLINDPATH, CLASS_ZEBRA, CLASS_MANHOLE, CLASS_OBSTACLE, CLASS_OTHER = 0, 1, 2, 3, 4
DET_CONF_THRESH = 0.45
DET_NMS_THRESH = 0.45
OBSTACLE_FOCAL_PX = 130.0
OBSTACLE_REAL_H_M = 0.8
WX_DRY, WX_WET = False, True

# ---------- 复刻 risk_of（obstacle_warning.cpp） ----------
def risk_of(dist, weather_wet):
    if weather_wet:
        if dist < 1.5: return 3
        if dist < 3.0: return 2
        if dist < 4.0: return 1
        return 0
    if dist < 1.0: return 3
    if dist < 2.0: return 2
    if dist < 3.0: return 1
    return 0

# ---------- 复刻 RecognizeResult 最小结构 ----------
class Res:
    def __init__(self):
        self.source = "LOCAL"
        self.path_detected = False
        self.path_status = "FOLLOWING"
        self.blindpath_conf = 0.0
        self.zebra_detected = False
        self.manhole_detected = False
        self.obstacle_cnt = 0
        self.obstacles = []
        self.det_conf = 0.0
        self.weather = "UNKNOWN"

# ---------- IoU + 解码（复刻 ondevice_detect.cpp::local_detect_run 后处理） ----------
def iou(a, b):
    x1 = max(a['x']-a['w']/2, b['x']-b['w']/2); x2 = min(a['x']+a['w']/2, b['x']+b['w']/2)
    y1 = max(a['y']-a['h']/2, b['y']-b['h']/2); y2 = min(a['y']+a['h']/2, b['y']+b['h']/2)
    iw = max(0.0, x2-x1); ih = max(0.0, y2-y1)
    inter = iw*ih; uni = a['w']*a['h'] + b['w']*b['h'] - inter
    return inter/uni if uni > 0 else 0

def decode(dets, src_w, src_h, weather_wet):
    """dets: list of dict {x,y,w,h,conf,cls}(归一化) -> Res"""
    out = Res()
    keep = []
    for d in dets:
        if d['conf'] < DET_CONF_THRESH:
            continue
        dup = False
        for k in keep:
            if k['cls'] == d['cls'] and iou(k, d) > DET_NMS_THRESH:
                if d['conf'] > k['conf']:
                    keep[keep.index(k)] = d
                dup = True; break
        if not dup:
            keep.append(d)
    max_conf = max([d['conf'] for d in keep], default=0.0)
    out.det_conf = max_conf

    best_bp, bp_box, has_bp = 0.0, None, False
    for d in keep:
        if d['cls'] == CLASS_BLINDPATH:
            out.path_detected = True
            out.blindpath_conf = d['conf']
            if d['conf'] > best_bp:
                best_bp = d['conf']; bp_box = d; has_bp = True
        elif d['cls'] == CLASS_ZEBRA:
            out.zebra_detected = True
        elif d['cls'] == CLASS_MANHOLE:
            out.manhole_detected = True
            if out.obstacle_cnt < 4:
                out.obstacles.append({"type":"manhole","dir":("left" if d['x']<0.4 else ("right" if d['x']>0.6 else "center")),
                                      "dist": OBSTACLE_FOCAL_PX*0.3/(d['h']*src_h), "risk":1})
                out.obstacle_cnt += 1
        elif d['cls'] == CLASS_OBSTACLE:
            if out.obstacle_cnt < 4:
                dist = OBSTACLE_FOCAL_PX*OBSTACLE_REAL_H_M/(d['h']*src_h)
                out.obstacles.append({"type":"obstacle","dir":("left" if d['x']<0.4 else ("right" if d['x']>0.6 else "center")),
                                      "dist": dist, "risk": risk_of(dist, weather_wet)})
                out.obstacle_cnt += 1
    if has_bp:
        if bp_box['x'] < 0.40: out.path_status = "OFFSET_LEFT"
        elif bp_box['x'] > 0.60: out.path_status = "OFFSET_RIGHT"
        else: out.path_status = "FOLLOWING"
    else:
        out.path_status = "FOLLOWING"
    out.source = "LOCAL"
    return out

# ---------- 离线闭环决策（复刻 main.cpp loop 的选择逻辑） ----------
def closed_loop_decision(local_ready, dets, weather_wet, src_w=320, src_h=240):
    """返回 (final_source, summary_str)
    离线闭环：端侧识别为主路径(SRC_LOCAL)，无模型/未推理则超声波安全底线(SRC_ULTRA)。
    注意：超声波安全底线在主循环中与视觉结果“融合”，此处只判定主来源。"""
    if local_ready and len(dets) > 0:
        res = decode(dets, src_w, src_h, weather_wet)
        if res.det_conf > 0:
            return "LOCAL", (f"端侧识别: 来源LOCAL 状态={res.path_status} 障碍数={res.obstacle_cnt} "
                             f"斑马线={res.zebra_detected} 井盖={res.manhole_detected} conf={res.det_conf:.2f}")
    # 端侧无模型或本周期无有效检测 -> 超声波安全底线
    return "ULTRA", "超声波安全底线(SRC_ULTRA)"

# ===================== 测试 =====================
def approx(a, b, tol=1e-6): return abs(a-b) <= tol

def test_decode_blindpath_offset():
    dets = [{"x":0.25,"y":0.5,"w":0.3,"h":0.1,"conf":0.9,"cls":CLASS_BLINDPATH}]
    r = decode(dets, 320, 240, WX_DRY)
    assert r.path_detected and r.path_status == "OFFSET_LEFT", r.path_status
    assert approx(r.blindpath_conf, 0.9)
    print("[PASS] 盲道左偏检测")

def test_decode_zebra_manhole():
    dets = [
        {"x":0.5,"y":0.6,"w":0.8,"h":0.2,"conf":0.8,"cls":CLASS_ZEBRA},
        {"x":0.7,"y":0.8,"w":0.1,"h":0.05,"conf":0.7,"cls":CLASS_MANHOLE},
    ]
    r = decode(dets, 320, 240, WX_DRY)
    assert r.zebra_detected and r.manhole_detected
    assert r.obstacle_cnt == 1 and r.obstacles[0]["type"] == "manhole"
    # 井盖距离 = 130*0.3/(0.05*240) = 39/12 = 3.25 m
    assert approx(r.obstacles[0]["dist"], 3.25, 1e-3), r.obstacles[0]["dist"]
    assert r.obstacles[0]["risk"] == 1  # 3.25m 正常档 LV1
    print("[PASS] 斑马线 + 井盖检测 + 距离估算")

def test_decode_obstacle_distance_weather():
    # 框高 h=0.2 (归一化) -> dist = 130*0.8/(0.2*240)=104/48=2.167 m
    dets = [{"x":0.5,"y":0.5,"w":0.1,"h":0.2,"conf":0.85,"cls":CLASS_OBSTACLE}]
    r_dry = decode(dets, 320, 240, WX_DRY)
    assert approx(r_dry.obstacles[0]["dist"], 2.167, 1e-3)
    assert r_dry.obstacles[0]["risk"] == 1  # 2.167m 正常档 <3m LV1
    r_wet = decode(dets, 320, 240, WX_WET)
    assert r_wet.obstacles[0]["risk"] == 2  # 湿滑档 <3m LV2（更保守）
    print("[PASS] 障碍距离 + 天气模式风险切换")

def test_decode_lowconf_filtered():
    dets = [{"x":0.5,"y":0.5,"w":0.1,"h":0.2,"conf":0.30,"cls":CLASS_OBSTACLE}]  # <阈值
    r = decode(dets, 320, 240, WX_DRY)
    assert r.obstacle_cnt == 0 and r.det_conf == 0.0, (r.obstacle_cnt, r.det_conf)
    print("[PASS] 低置信检测被阈值过滤")

def test_nms_duplicate():
    dets = [
        {"x":0.5,"y":0.5,"w":0.2,"h":0.2,"conf":0.6,"cls":CLASS_OBSTACLE},
        {"x":0.52,"y":0.5,"w":0.2,"h":0.2,"conf":0.9,"cls":CLASS_OBSTACLE},  # 重叠更高置信
    ]
    r = decode(dets, 320, 240, WX_DRY)
    # 同类重叠 IoU>NMS阈 -> 仅保留更高置信 1 个
    assert r.obstacle_cnt == 1
    assert approx(r.obstacles[0]["dist"], 130*0.8/(0.2*240), 1e-3)
    print("[PASS] 同类 NMS 取高置信")

def test_closedloop_ultra_baseline():
    # 无有效检测（含无模型场景）-> 超声波安全底线
    src, msg = closed_loop_decision(local_ready=True, dets=[], weather_wet=WX_DRY)
    assert src == "ULTRA", msg
    src2, _ = closed_loop_decision(local_ready=False, dets=[], weather_wet=WX_DRY)
    assert src2 == "ULTRA", msg
    print("[PASS] 离线闭环-无模型/无效检测走超声波底线")

def test_closedloop_local_primary():
    dets = [{"x":0.5,"y":0.5,"w":0.3,"h":0.1,"conf":0.9,"cls":CLASS_BLINDPATH}]
    src, msg = closed_loop_decision(local_ready=True, dets=dets, weather_wet=WX_DRY)
    assert src == "LOCAL", msg
    print("[PASS] 离线闭环-端侧识别优先(正常置信)")

def test_closedloop_lowconf_to_ultra():
    # 仅低置信(<阈值)检测 -> 解码被过滤 -> 无有效检测 -> 超声波底线
    dets = [{"x":0.5,"y":0.5,"w":0.3,"h":0.1,"conf":0.30,"cls":CLASS_BLINDPATH}]
    src, msg = closed_loop_decision(local_ready=True, dets=dets, weather_wet=WX_DRY)
    assert src == "ULTRA", msg
    print("[PASS] 离线闭环-端侧低置信被过滤后回退超声波")

if __name__ == "__main__":
    tests = [v for k,v in sorted(globals().items()) if k.startswith("test_")]
    passed = 0
    for t in tests:
        try:
            t(); passed += 1
        except AssertionError as e:
            print(f"[FAIL] {t.__name__}: {e}")
        except Exception as e:
            print(f"[ERR ] {t.__name__}: {e}")
    print(f"\n端侧解码+离线闭环决策仿真：{passed}/{len(tests)} 通过")
