# -*- coding: utf-8 -*-
"""
test_client.py —— 云端接口联调测试客户端（阶段七 / Postman 替代）
用法：
    python test_client.py <图片路径> [服务器地址]
例：
    python test_client.py test.jpg http://192.168.1.10:5000
发送协议一致的 JSON，打印返回的识别结果，用于验证 08 云端服务是否联通。
"""
import sys, base64, json, requests

def main():
    if len(sys.argv) < 2:
        print("用法: python test_client.py <图片路径> [服务器地址]")
        return
    img_path = sys.argv[1]
    url = sys.argv[2] if len(sys.argv) > 2 else "http://127.0.0.1:5000"
    with open(img_path, "rb") as f:
        b64 = base64.b64encode(f.read()).decode()
    payload = {
        "device_id": "TEST_CLIENT",
        "image_base64": b64,
        "ultrasonic_cm": -1,
        "battery": 88,
        "ts": 0,
    }
    try:
        r = requests.post(url + "/api/recognize", json=payload, timeout=10)
        print("HTTP", r.status_code)
        print(json.dumps(r.json(), ensure_ascii=False, indent=2))
    except Exception as e:
        print("请求失败:", e)

if __name__ == "__main__":
    main()
