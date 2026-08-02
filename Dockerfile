# 盲道识别云端服务 —— 一键容器化部署
# 构建并运行：docker compose up --build -d
FROM python:3.11-slim

WORKDIR /app

# 仅复制依赖清单先装，利用层缓存加速重建
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# 复制全部服务源码（app/llm_client/prompts/rate_limiter/logger）
COPY . .

# 默认用 mock 供应商（无需 API Key 即可端到端验证）；生产改为 tongyi/deepseek
ENV LLM_PROVIDER=mock
ENV PORT=5000

EXPOSE 5000

# 生产用 gunicorn；2 worker 适配 2核4G
CMD ["gunicorn", "-w", "2", "-b", "0.0.0.0:5000", "app:app"]
