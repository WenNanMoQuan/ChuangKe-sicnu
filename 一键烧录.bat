@echo off
chcp 65001 >nul
REM ============================================================================
REM  盲道智能引导系统 —— 终端一键烧录（Windows）
REM ============================================================================
REM  用法（双击本文件即默认“编译+上传+监控”）：
REM    一键烧录.bat            编译 + 上传 + 打开串口监控
REM    一键烧录.bat build      仅编译
REM    一键烧录.bat upload     仅上传（需先 build）
REM    一键烧录.bat monitor    仅串口监控
REM    一键烧录.bat model      提示如何把模型放入 TF 卡
REM
REM  纯离线闭环固件：无需任何 WiFi/云端即可运行。
REM ============================================================================
cd /d "%~dp0"

REM ---------- 1. 自动安装 PlatformIO（若缺失） ----------
where pio >nul 2>nul
if errorlevel 1 (
  echo [*] 未检测到 PlatformIO，正在自动安装（需要已装 Python3）...
  python -m pip install -U platformio
  if errorlevel 1 (
    echo [ERR] 安装失败，请先安装 Python3 后手动执行: python -m pip install -U platformio
    pause
    exit /b 1
  )
)

REM ---------- 2. 模型提示子命令 ----------
if "%1"=="model" (
  echo [*] 请把训练/转换好的 detect.tflite 放进 TF 卡 /model/ 目录：
  echo     - 在电脑读出 TF 卡，建 model 文件夹
  echo     - 复制 detect.tflite 到 model\detect.tflite
  echo     - 没放也能烧录运行（自动走超声波安全底线），放好即启用端侧 AI
  pause
  exit /b 0
)

REM ---------- 3. 执行动作 ----------
if "%1"=="build" (
  pio run
) else if "%1"=="upload" (
  pio run -t upload
) else if "%1"=="monitor" (
  pio device monitor -b 115200
) else (
  pio run -t upload
  if not errorlevel 1 (
    pio device monitor -b 115200
  )
)
pause
