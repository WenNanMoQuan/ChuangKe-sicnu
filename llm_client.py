"""
llm_client.py —— 多模态视觉大模型调用封装
阶段四：接入国产多模态视觉大模型 API（通义千问 VL / DeepSeek-VL）

通过环境变量切换供应商与鉴权：
  export LLM_PROVIDER=tongyi        # tongyi | deepseek
  export DASHSCOPE_API_KEY=sk-xxx   # 通义千问
  export DEEPSEEK_API_KEY=sk-xxx    # DeepSeek

提示词来自 09_提示词工程/prompts.py（V3 为最终版）。
"""
import os, json, requests
from prompts import PROMPT_V3   # 阶段六迭代到 V3.0

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
    url, model, key, auth = ENDPOINTS[PROVIDER]
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
