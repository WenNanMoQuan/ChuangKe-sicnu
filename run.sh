#!/usr/bin/env bash
# 盲道引导系统 —— 统一终端总控（纯命令行入口）
set -e
cd "$(dirname "$0")"

PY=python3
command -v python3 >/dev/null 2>&1 || PY=python

ensure_pio() {
  if ! "$PY" -m platformio --version >/dev/null 2>&1; then
    echo "[*] 未检测到 PlatformIO，正在 pip 安装（首次约需几分钟）..."
    "$PY" -m pip install -U platformio
  fi
}

case "${1:-help}" in
  build)
    ensure_pio
    ( cd firmware && "$PY" -m platformio run )
    ;;
  flash|upload)
    ensure_pio
    ( cd firmware && "$PY" -m platformio run -t upload )
    ;;
  monitor)
    ensure_pio
    ( cd firmware && "$PY" -m platformio device monitor )
    ;;
  test)
    ( cd tools/tests && "$PY" run_tests.py )
    ;;
  model)
    echo "[*] 模型转换工具：将 ONNX/YOLO 权重转 int8 TFLite，产物放到 TF 卡 /model/detect.tflite"
    ( cd tools/model_tools && "$PY" convert_to_tflite.py --help )
    ;;
  cloud)
    echo "[*] 启动开发可选遥测云端（默认 mock 模式，无需 API Key）..."
    ( cd tools/cloud && "$PY" app.py )
    ;;
  help|*)
    cat <<'EOF'
盲道引导系统 —— 统一终端总控（纯命令行，无图形界面依赖）
用法: ./run.sh <子命令>

  build     编译固件 (PlatformIO)
  flash     编译并烧录到 ESP32-S3
  monitor   打开串口监视器
  test      运行 Python 等价仿真测试（35 项）
  model     模型转换工具说明 / 帮助
  cloud     启动开发可选遥测云端 (Flask, 默认 mock)
  help      显示本帮助

目录:
  firmware/          离线闭环固件 (PlatformIO 终端工程，去 Arduino IDE 依赖)
  tools/tests/       Python 等价仿真与测试
  tools/model_tools/ ONNX -> int8 TFLite 转换
  tools/cloud/       开发可选遥测云端 (Flask)
EOF
    ;;
esac
