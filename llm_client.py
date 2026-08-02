"""
llm_client.py —— 多模态视觉大模型调用封装
阶段四：接入国产多模态视觉大模型 API（通义千问 VL / DeepSeek-VL）

通过环境变量切换供应商与鉴权：
  export LLM_PROVIDER=tongyi        # tongyi | deepseek
  export DASHSCOPE_API_KEY=sk-xxx   # 通义千问
  export DEEPSEEK_API_KEY=sk-xxx    # DeepSeek

提示词来自 09_提示词工程/prompts.py（V3 为最终版）。
"""
import os, json, sys
# prompts.py 与本文件同目录；若被其他目录（如 10_测试与验证）导入，
# 确保把本文件所在目录加入搜索路径，避免 ModuleNotFoundError。
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from prompts import PROMPT_V3   # 阶段六迭代到 V3.0

# requests 仅在调用真实大模型时才需要；mock/离线模式不依赖，故延迟导入，
# 避免本机无网络无法 pip 安装 requests 时整模块不可用。

PROVIDER = os.getenv("LLM_PROVIDER", "tongyi")
DASHSCOPE_KEY = os.getenv("DASHSCOPE_API_KEY", "")
DEEPSEEK_KEY  = os.getenv("DEEPSEEK_API_KEY", "")
TIMEOUT = 5  # 阶段四：>5秒视为超时，触发降级

ENDPOINTS = {
    "tongyi":  ("https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions",
                "qwen2.5-vl-72b-instruct", DASHSCOPE_KEY, f"Bearer {DASHSCOPE_KEY}"),
    "deepseek": ("https://api.deepseek.com/v1/chat/completions",
                 "deepseek-vl2", DEEPSEEK_KEY, f"Bearer {DEEPSEEK_KEY}"),
}

def recognize_image(image_base64: str) -> dict:
    """调用视觉大模型，解析为结构化识别结果（严格 JSON）。"""
    # 运行时动态读取供应商：既支持启动期用 LLM_PROVIDER 设定默认，
    # 也允许在不重启进程的情况下切换（如 mock <-> tongyi）。
    provider = os.getenv("LLM_PROVIDER", PROVIDER).lower()
    # 本地仿真/离线测试模式：无需真实 API Key，返回协议一致的结构化样例，
    # 用于验证端云管线（限流/日志/解析/降级）是否通畅。
    if provider == "mock":
        return {
            "blind_path": {"detected": True, "offset_direction": "center",
                           "offset_angle": 0, "status": "FOLLOWING"},
            "obstacles": [{"type": "pedestrian", "distance": 1.5,
                           "direction": "center", "risk_level": 2}],
            "weather": "clear",
            "feedback": {"vibration": 60, "buzzer": True,
                         "led_color": 16753920, "voice_text": "前方障碍较近"},
        }
    import requests  # 延迟导入：仅真实调用时依赖
    url, model, key, auth = ENDPOINTS[provider]
    headers = {"Authorization": auth, "Content-Type": "application/json"}
    payload = {
        "model": model,
        "messages": [{
            "role": "user",
            "content": [
                {"type": "text", "text": PROMPT_V3},
                {"type": "image_url", "image_url": {
                    "url": f"data:image/jpeg;base64,{image_base64}"}},
            ],
        }],
        "response_format": {"type": "json_object"},
        "temperature": 0.2,
    }
    r = requests.post(url, headers=headers, json=payload, timeout=TIMEOUT)
    r.raise_for_status()
    content = r.json()["choices"][0]["message"]["content"]
    return json.loads(content)  # 提示词已约束为协议所需 JSON
