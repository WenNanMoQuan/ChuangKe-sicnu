@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

set PY=python
where python >nul 2>&1 || set PY=py

if "%1"=="" goto help
if "%1"=="build" (
  call :ensure_pio
  cd firmware
  %PY% -m platformio run
  goto end
)
if "%1"=="flash" (
  call :ensure_pio
  cd firmware
  %PY% -m platformio run -t upload
  goto end
)
if "%1"=="upload" (
  call :ensure_pio
  cd firmware
  %PY% -m platformio run -t upload
  goto end
)
if "%1"=="monitor" (
  call :ensure_pio
  cd firmware
  %PY% -m platformio device monitor
  goto end
)
if "%1"=="test" (
  cd tools/tests
  %PY% run_tests.py
  goto end
)
if "%1"=="model" (
  echo [*] 模型转换工具：将 ONNX/YOLO 权重转 int8 TFLite，产物放到 TF 卡 /model/detect.tflite
  cd tools/model_tools
  %PY% convert_to_tflite.py --help
  goto end
)
if "%1"=="cloud" (
  echo [*] 启动开发可选遥测云端（默认 mock 模式，无需 API Key）
  cd tools/cloud
  %PY% app.py
  goto end
)
goto help

:ensure_pio
%PY% -m platformio --version >nul 2>&1
if errorlevel 1 (
  echo [*] 未检测到 PlatformIO，正在 pip 安装...
  %PY% -m pip install -U platformio
)
goto :eof

:help
echo 盲道引导系统 —— 统一终端总控（纯命令行，无图形界面依赖）
echo 用法: run.bat ^<子命令^>
echo   build   编译固件
echo   flash   编译并烧录
echo   monitor 打开串口监视器
echo   test    运行 Python 等价仿真测试
echo   model   模型转换工具说明
echo   cloud   启动开发可选遥测云端
echo   help    显示本帮助

:end
endlocal
