# 终端一键烧录程序（盲道智能引导系统 · 离线闭环固件）

把整套可烧录固件 + 一键脚本打包在一起，**双击/运行一个脚本即可编译烧录到 ESP32-S3**，无需手动拼文件。

## 这是什么
- 设备**完全脱离网络**：所有识别与处理都在 ESP32-S3 单片机闭环完成。
- 识别主路径：端侧 TFLite Micro（OV2640 → 盲道/斑马线/井盖/障碍检测）。
- 安全底线：HC-SR04 超声波（10Hz，与视觉融合；无模型时作为唯一兜底）。
- 天气湿滑档：由**物理按钮**本地切换（不依赖云端）。

## 一键烧录（三选一）
1. **Windows**：双击 `一键烧录.bat`（自动装 PlatformIO → 编译 → 上传 → 监控）。
2. **Linux / macOS / WSL**：终端执行 `./一键烧录.sh`（需先 `chmod +x 一键烧录.sh`）。
3. **Arduino IDE**：直接打开 `BlindGuide_Firmware.ino`，选 `ESP32S3 Dev Module`、`PSRAM=Enabled(OPI)`、`Flash 8MB`，点上传。

子命令：`build`（仅编译） / `upload`（仅上传） / `monitor`（仅监控） / `model`（模型放置提示）。

## 烧录前必做（对照实物板）
1. 保持 `config.h` 中 `ENABLE_WIFI_TELEMETRY=0`（纯离线，默认即如此）。
2. **★ 核对 LCD 7 引脚、TF 卡 4 引脚、摄像头 XCLK(GPIO9)** 与正点原子 DNEP32S3 官方例程一致。
3. **Echo 分压**：HC-SR04 的 Echo(43) 是 5V，必须分压到 3.3V，否则烧 IO！
4. **电池分压**：电池正极与 GND 间接 2 个等值电阻（如 100k+100k）分压，中点接 GPIO8；勿直连 3.7V！
5. **语音文件（可选）**：TF 卡建 `/voice/`，放 16kHz/16bit/单声道 WAV（left/right/turn/end/obs/safe）。没有也能用（退化为蜂鸣）。

## 端侧模型（可选但推荐）
把转换好的 `detect.tflite` 放到 TF 卡 `/model/detect.tflite`。**没放也能烧录运行**（自动走超声波安全底线）；放好即自动启用端侧 AI，无需改代码。模型训练/转换见 `11_端侧AI识别/model_tools/`。

## 运行日志（串口 115200）
- 看到 `[AI] 模型加载成功 ...` → 端侧 AI 已启用（SRC_LOCAL）。
- 看到 `[AI] 模型不存在 ... 走超声波安全底线` → 无模型，纯超声波兜底（SRC_ULTRA），正常可用。
- `[WX] button toggle -> wet=1` → 物理按钮切换雨天/湿滑更保守预警档。
