"""
logger.py —— 请求/响应归档日志（阶段四：按设备ID和时间归档，便于事后分析）
日志写入 ./logs/<device_id>/<YYYY-MM-DD>.log
"""
import os, time, json

LOG_DIR = os.getenv("LOG_DIR", "./logs")

def log_request(device_id, req, resp, elapsed, status):
    day = time.strftime("%Y-%m-%d")
    dpath = os.path.join(LOG_DIR, str(device_id))
    os.makedirs(dpath, exist_ok=True)
    line = {
        "ts": time.strftime("%Y-%m-%d %H:%M:%S"),
        "device_id": device_id,
        "elapsed_s": round(elapsed, 3),
        "status": status,
        "ultrasonic_cm": req.get("ultrasonic_cm"),
        "battery": req.get("battery"),
        "resp": resp,
    }
    with open(os.path.join(dpath, f"{day}.log"), "a", encoding="utf-8") as f:
        f.write(json.dumps(line, ensure_ascii=False) + "\n")
