# BlindGuide_Firmware —— 一键烧录固件工程

把 `01~07` 各功能模块**扁平化拼装**成单个 Arduino 工程，**直接用 Arduino IDE 或 PlatformIO 打开即可编译烧录**，无需再手动拼文件。

> 本工程是 `01~07` 源码的**副本快照**。若你修改了上层 `01~07` 模块，请重新把它们对应的 `.h/.cpp` 复制进本目录（保持同名覆盖）。天气模式、引脚、Base64 内置等已与上层源码保持一致。

## 目录内容
```
BlindGuide_Firmware/
├─ BlindGuide_Firmware.ino   # 总装入口（原 07/main.cpp，含 setup/loop）
├─ config.h / protocol.h     # 01 引脚与协议
├─ camera_capture.h/.cpp     # 02 OV2640 采集（含 base64_util.h 内置编码）
├─ network_manager.h/.cpp    # 03 WiFi/HTTP/解析
├─ peripherals.h/.cpp        # 04 震动/灯带/蜂鸣/超声波/I2S 语音
├─ display_ui.h/.cpp         # 05 LovyanGFX+LVGL 屏显
├─ core_logic.h              # 06 接口集合（含天气模式 API）
├─ guidance_fsm.cpp          # 06 盲道状态机
├─ obstacle_warning.cpp      # 06 障碍分级 + 天气模式 risk_of
├─ offline_fallback.cpp      # 06 离线超声波兜底
├─ lv_conf.h                 # LVGL 8.3 配置（必须随 sketch 一起）
├─ platformio.ini            # PlatformIO 一键命令行烧录配置
└─ README.md
```

## 烧录前必做（对照实物板）
1. **改 WiFi/云端**：打开 `config.h`，把 `WIFI_SSID / WIFI_PASSWORD / CLOUD_API_URL` 改成你的。
2. **★ 核对 7 个 LCD 引脚**（`LCD_SCK_PIN`…`LCD_RST_PIN`）与 **TF 卡 4 个引脚**（`SD_*`）、**摄像头 XCLK**（GPIO9）是否与你手里的**正点原子 DNEP32S3 官方例程**一致。代码里是“无冲突占位默认值”，必须与官方原理图最终对齐，否则取图/屏幕/读卡会失败。
3. **Echo 分压**：HC-SR04 的 Echo(43) 是 5V，必须分压到 3.3V，否则烧毁 IO！
4. **电池分压**：在电池正极与 GND 间接 2 个等值电阻（如 100k+100k）分压，中点接 `BATTERY_ADC_PIN`(GPIO8)；勿直连 3.7V！
5. **语音文件**（可选）：在 TF 卡根目录建 `/voice/`，放入 16kHz/16bit/单声道 WAV：`left.wav right.wav turn.wav end.wav obs.wav safe.wav`。没有也能用（退化为蜂鸣）。

## 方式 A：Arduino IDE（推荐新手）
1. 菜单 **文件 → 打开** → 选本目录的 `BlindGuide_Firmware.ino`。
2. 安装库（库管理搜索）：`Adafruit NeoPixel`、`ArduinoJson`、`lvgl`(8.3.x)、`LovyanGFX`。
   - `esp_camera/WiFi/HTTPClient/BLE/SD/SPI` 随板级包自带；`Base64` 已内置，**不用装**。
3. 工具 → 开发板选 **ESP32S3 Dev Module**；**PSRAM 选 "Enabled(OPI)"**；Flash 选 8MB；端口选对的 COM。
4. 点 **上传**。串口监视器 115200 看日志。

## 方式 B：PlatformIO（VS Code）
1. 装 PlatformIO 插件，打开本目录。
2. 点侧边栏 **Build**（首次自动安装 `lib_deps` 里的库）。
3. **Upload** 烧录，**Monitor** 看日志（`monitor_speed=115200`）。

## 天气模式（雨天/湿滑预警）
- 由 `obstacle_warning.cpp` 的 `risk_of()` + 全局 `g_weather_mode` 实现：
  - 正常档：距离 <1m LV3 / <2m LV2 / <3m LV1；
  - **湿滑档（阈值整体 +1m 更保守）**：<1.5m LV3 / <3m LV2 / <4m LV1；离线超声波阈值 50→80cm。
- 编译期默认 `WEATHER_MODE_DEFAULT false`（见 `config.h`）。
- 运行时切换：在 `BlindGuide_Firmware.ino` 的 `setup()` 已调用 `set_weather_mode(WEATHER_MODE_DEFAULT)`；需要按天气动态切换时，在 `loop()` 里检测云端天气字段或物理开关后调用 `set_weather_mode(true/false)` 即可。

## 验证（本机无 ESP32 工具链，用宿主机仿真代替）
上层 `10_测试与验证/` 提供等价 Python 仿真：
```
cd ../10_测试与验证
python run_tests.py        # 功能测试（含雨天组、天气模式一致性）
python sim_core_logic.py   # 决策场景仿真（含雨天退化多变验证）
```
