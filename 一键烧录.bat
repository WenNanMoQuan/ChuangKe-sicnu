@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

set PY=python
where python >nul 2>&1 || set PY=py

:: 自动安装 PlatformIO（若本机缺失）
%PY% -m platformio --version >nul 2>&1
if errorlevel 1 (
  echo [*] 未检测到 PlatformIO，正在 pip 安装...
  %PY% -m pip install -U platformio
)

if "%1"=="" goto doflash
if "%1"=="build" (
  %PY% -m platformio run
  goto end
)
if "%1"=="flash" goto doflash
if "%1"=="upload" goto doflash
if "%1"=="monitor" (
  %PY% -m platformio device monitor
  goto end
)
echo 用法: 一键烧录.bat [build^|flash^|monitor]
goto end

:doflash
%PY% -m platformio run -t upload

:end
endlocal
