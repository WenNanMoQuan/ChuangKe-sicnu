# 08 云端识别服务

> 对应规划：阶段四（云端服务与 AI 识别能力搭建）

## 一、功能
基于 Flask 的轻量云端中转服务，提供统一接口 `POST /api/recognize`：
- 接收设备 POST 的 Base64 图片（及超声波/电量）。
- 调用国产多模态视觉大模型（通义千问 VL / DeepSeek-VL）做盲道+障碍识别。
- 输出**严格 JSON** 结构化结果，字段与端侧 `protocol.h` 完全对齐。
- 内置三大保障：
  - **限流**：令牌桶，每设备 1 次/秒（防刷爆 API 额度）。
  - **日志**：按 `device_id/日期` 归档所有请求与响应，便于分析。
  - **降级**：大模型超时(>5s)/异常时返回 `degraded:true`；连续 3 次降级下发 `offline_suggest:true`，设备据此切离线超声波兜底。

## 二、作用
- 系统的“AI 大脑”：把端侧拍到的图变成结构化引导指令。
- 与 07 端侧固件通过 `protocol.h` 定义的 JSON 协议对接，是端云协同的另一端。

## 三、拼接方法
1. 安装依赖：`pip install -r requirements.txt`。
2. 配置环境变量（鉴权与供应商切换）：
   ```bash
   export LLM_PROVIDER=tongyi
   export DASHSCOPE_API_KEY=sk-xxxx      # 通义千问
   # 或 export LLM_PROVIDER=deepseek + DEEPSEEK_API_KEY=sk-xxxx
   ```
3. 启动：`gunicorn -w 2 -b 0.0.0.0:5000 app:app`（生产）或 `python app.py`（调试）。
4. 把服务部署到 2核4G 云服务器，防火墙放行 5000 端口；端侧 `config.h` 的 `CLOUD_API_URL` 指向 `http://<服务器IP>:5000/api/recognize`。
5. **提示词依赖 09_提示词工程**：`llm_client.py` 通过 `from prompts import PROMPT_V3` 引用最终版提示词；部署时把 `09_提示词工程/prompts.py` 与本服务放同一目录（或加入 PYTHONPATH）。
6. 接口联调可用 Postman 发送一张测试图（见 10_测试与验证）。
