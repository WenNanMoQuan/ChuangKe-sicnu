#!/usr/bin/env bash
# 盲道引导固件 —— 一键烧录（纯终端 / PlatformIO 命令行）
set -e
cd "$(dirname "$0")"

PY=python3
command -v python3 >/dev/null 2>&1 || PY=python

# 自动安装 PlatformIO（若本机缺失）
if ! "$PY" -m platformio --version >/dev/null 2>&1; then
  echo "[*] 未检测到 PlatformIO，正在 pip 安装（首次约需几分钟）..."
  "$PY" -m pip install -U platformio
fi

case "${1:-flash}" in
  build)
    "$PY" -m platformio run
    ;;
  flash|upload)
    "$PY" -m platformio run -t upload
    ;;
  monitor)
    "$PY" -m platformio device monitor
    ;;
  *)
    echo "用法: ./一键烧录.sh [build|flash|monitor]"
    echo "  build   仅编译"
    echo "  flash   编译并烧录（默认）"
    echo "  monitor 打开串口监视器"
    ;;
esac
