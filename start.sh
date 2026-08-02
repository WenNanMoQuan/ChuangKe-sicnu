#!/usr/bin/env bash
# 盲道识别云端服务 —— 一键启动脚本
# 优先使用 docker compose（Docker v2），回退到 docker-compose（Docker v1）
set -e

if ! command -v docker >/dev/null 2>&1; then
  echo "未检测到 Docker，请先安装：https://docs.docker.com/get-docker/" >&2
  exit 1
fi

if docker compose version >/dev/null 2>&1; then
  echo ">>> 使用 docker compose 构建并启动 ..."
  docker compose up --build -d
else
  echo ">>> 使用 docker-compose 构建并启动 ..."
  docker-compose up --build -d
fi

echo ">>> 已启动。健康检查："
echo "    curl http://localhost:${CLOUD_PORT:-5000}/health"
