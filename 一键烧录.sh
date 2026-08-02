#!/usr/bin/env bash
# =============================================================================
#  盲道智能引导系统 —— 终端一键烧录（Linux / macOS / WSL）
# =============================================================================
#  用法：
#    ./一键烧录.sh            编译 + 自动探测端口上传 + 打开串口监控
#    ./一键烧录.sh build      仅编译（不烧录）
#    ./一键烧录.sh upload     仅上传（需先 build）
#    ./一键烧录.sh monitor    仅串口监控
#    ./一键烧录.sh model      提示如何把模型放入 TF 卡
#
#  本脚本会：① 缺 PlatformIO 自动装；② 自动找 ESP32-S3 串口；③ 一键烧录+监控。
#  纯离线闭环固件：无需任何 WiFi/云端即可运行。
# =============================================================================
set -e
cd "$(dirname "$0")"

# ---------- 1. 自动安装 PlatformIO（若缺失） ----------
if ! command -v pio >/dev/null 2>&1; then
  echo "[*] 未检测到 PlatformIO，正在自动安装（需要 Python3 + pip）..."
  python3 -m pip install -U platformio >/dev/null 2>&1 || {
    echo "[ERR] pip 安装失败，请手动执行: python3 -m pip install -U platformio"; exit 1; }
fi
export PATH="$HOME/.local/bin:$PATH"

# ---------- 2. 模型提示子命令 ----------
if [ "$1" = "model" ]; then
  echo "[*] 请把训练/转换好的 detect.tflite 放进 TF 卡 /model/ 目录："
  echo "    - 在电脑读出 TF 卡，建 /model 文件夹"
  echo "    - 复制 detect.tflite 到 /model/detect.tflite"
  echo "    - 没放也能烧录运行（自动走超声波安全底线），放好即启用端侧 AI"
  exit 0
fi

# ---------- 3. 自动探测 ESP32-S3 串口 ----------
detect_port() {
  # Linux
  for p in /dev/ttyUSB* /dev/ttyACM*; do [ -e "$p" ] && { echo "$p"; return; }; done
  # macOS (CP210x / CH34x)
  for p in /dev/cu.SLAB_USBtoUART /dev/cu.usbserial* /dev/cu.wchusbserial*; do
    [ -e "$p" ] && { echo "$p"; return; }
  done
  echo ""
}
PORT="$(detect_port)"
if [ -z "$PORT" ]; then
  echo "[WARN] 未自动发现串口，请确认 ESP32-S3 已用数据线连接电脑。"
  echo "        仍将继续，若烧录失败请手动指定： pio run -t upload --upload-port /dev/ttyXXX"
fi

# ---------- 4. 执行动作 ----------
if [ "$1" = "build" ]; then
  pio run
elif [ "$1" = "upload" ]; then
  pio run -t upload ${PORT:+--upload-port "$PORT"}
elif [ "$1" = "monitor" ]; then
  pio device monitor -b 115200
else
  if [ -n "$PORT" ]; then
    pio run -t upload --upload-port "$PORT" && pio device monitor -b 115200
  else
    pio run -t upload && pio device monitor -b 115200
  fi
fi
