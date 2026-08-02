"""
app.py —— 盲道识别云端服务（Flask）
阶段四：云端服务与 AI 识别能力搭建

提供统一接口  POST /api/recognize
请求(JSON): { device_id, image_base64, ultrasonic_cm, battery, ts }
响应(JSON): {
    blind_path: { detected, offset_direction, offset_angle, status },
    obstacles:  [ { type, distance, direction, risk_level } ],
    feedback:   { vibration, buzzer, led_color, voice_text },
    degraded:   false
}

特性：请求限流(令牌桶,每设备1次/秒)、完整日志归档、异常降级策略。
"""
import os, time, json
from flask import Flask, request, jsonify
from llm_client import recognize_image
from rate_limiter import TokenBucket
from logger import log_request

app = Flask(__name__)

# 每设备每秒最多 1 次请求
BUCKETS = {}
def get_bucket(device_id):
    if device_id not in BUCKETS:
        BUCKETS[device_id] = TokenBucket(rate=1.0, capacity=1)
    return BUCKETS[device_id]

# 连续降级计数：>=3 次通知设备切离线超声波兜底
DEGRADE_CNT = {}

@app.route("/api/recognize", methods=["POST"])
def api_recognize():
    data = request.get_json(force=True, silent=True) or {}
    device_id = data.get("device_id", "unknown")
    image_b64 = data.get("image_base64", "")
    ultra = data.get("ultrasonic_cm", -1)
    battery = data.get("battery", -1)

    # —— 限流 ——
    if not get_bucket(device_id).consume():
        return jsonify({"degraded": True, "reason": "rate_limited"}), 429

    t0 = time.time()
    try:
        # 调用多模态大模型（阶段四：通义千问VL / DeepSeek-VL）
        result = recognize_image(image_b64)
        # 兜底：确保返回含 weather 字段（驱动端侧天气模式），缺失时按 clear 处理
        if "weather" not in result:
            result["weather"] = "clear"
        elapsed = time.time() - t0
        DEGRADE_CNT[device_id] = 0
        result["degraded"] = False
        log_request(device_id, data, result, elapsed, "OK")
        return jsonify(result)
    except Exception as e:
        # —— 异常降级：超时/报错返回降级标记，设备维持上次反馈 ——
        elapsed = time.time() - t0
        DEGRADE_CNT[device_id] = DEGRADE_CNT.get(device_id, 0) + 1
        fallback = {
            "blind_path": {"detected": False, "offset_direction": "center",
                           "offset_angle": 0, "status": "FOLLOWING"},
            "obstacles": [],
            "feedback": {"vibration": 0, "buzzer": False, "led_color": 0, "voice_text": ""},
            "degraded": True,
            "offline_suggest": DEGRADE_CNT[device_id] >= 3  # 连续3次降级->切离线
        }
        log_request(device_id, data, fallback, elapsed, f"ERROR:{e}")
        return jsonify(fallback), 200

@app.route("/health", methods=["GET"])
def health():
    return jsonify({"status": "up"})

if __name__ == "__main__":
    # 推荐 2核4G 云服务器；生产用 gunicorn 替代 flask 内置 server
    app.run(host="0.0.0.0", port=5000, threaded=True)
