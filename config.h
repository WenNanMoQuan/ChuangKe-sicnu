/**
 * @file    config.h
 * @brief   盲道智能引导系统 —— 全局硬件引脚分配与运行参数配置
 * @stage   阶段二：硬件原型搭建 / 阶段三：嵌入式基础软件开发
 *
 * 说明：
 *  - 引脚分配以《项目硬件模块清单.xlsx》的“引脚分配速查”表为权威依据；
 *    该表与《阶段规划_优化版.docx》存在个别冲突（详见下方注释），一律以本文件为准。
 *  - DVP 摄像头接口由正点原子 DNEP32S3 开发板硬件固定占用 GPIO10~GPIO20，
 *    具体引脚以官方 camera 例程为准（见 camera_capture 模块注释）。
 *  - 其余外设引脚为本项目自定义分配，修改后需同步更新接线原理图。
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/* ===================== 1. 摄像头 (OV2640, DVP) ===================== */
/* 引脚在 camera_capture.cpp 中按《硬件模块清单.xlsx》引脚速查表写死：
 *   数据 Y0~Y7 = GPIO10~17，PCLK=18，VSYNC=19，HREF=20，SCCB(I2C)=38/39，XCLK=9。
 *   这些由正点原子 DNEP32S3 板载 DVP FPC 硬件固定；
 *   ★ 请对照官方 camera 例程确认 XCLK/PWDN/RESET 是否与你的板子一致。
 *   帧缓冲已放入 8MB PSRAM（CAMERA_FB_IN_PSRAM），充分利用板载 PSRAM。 */

/* ===================== 2. DRV2605L 震动马达 (I2C) ===================== */
#define I2C_SDA_PIN        38
#define I2C_SCL_PIN        39
#define DRV2605_I2C_ADDR   0x5A

/* ===================== 3. 有源蜂鸣器 (GPIO 高电平驱动) ===================== */
#define BUZZER_PIN         40

/* ===================== 4. WS2812B 灯带 (单线协议) ===================== */
#define LED_PIN            41
#define LED_COUNT          8

/* ===================== 5. HC-SR04 超声波 (Trig + Echo) ===================== */
/* ⚠️ 源文档引脚冲突说明：
 *    《阶段规划_优化版.docx》写明 Echo=GPIO2；
 *    《项目硬件模块清单.xlsx》引脚分配速查表写明 Echo=GPIO43（并备注“Echo 需分压至 3.3V”）。
 *    本文件以 xlsx 引脚速查表为准，采用 GPIO43。若你的实际硬件按 docx 接在了 GPIO2，
 *    请把下面这一行改成 2。Echo 是 5V 输出，ESP32 的 GPIO 为 3.3V 容忍，
 *    必须在 Echo 线上串联 1kΩ+2kΩ 分压（或用电平转换），否则会烧毁 IO！ */
#define ULTRA_TRIG_PIN     42
#define ULTRA_ECHO_PIN     43

/* ===================== 6. MAX98357A I2S 功放 (三线) ===================== */
#define I2S_BCLK_PIN       4
#define I2S_LRCLK_PIN      5
#define I2S_DIN_PIN        6

/* ===================== 7. LCD (SPI, 2.4寸 ATK-2.4-TFT ILI9341) ===================== */
/* ★ 板上 LCD 通过板载 FPC 连接，引脚由正点原子 DNEP32S3 官方原理图固定。
 *   下面为“无冲突占位默认值”（避开摄像头 9~20 / I2C 38,39 / I2S 4,5,6 /
 *   蜂鸣40 / 灯带41 / 超声42,43 / XCLK9 / 电池8 / PSRAM 区），供 display 模块引用。
 *   ⚠️ 烧录前必须对照正点原子官方 LCD 例程核对这 7 个引脚，不一致请改！ */
#define LCD_SCK_PIN   21
#define LCD_MOSI_PIN  22
#define LCD_MISO_PIN  23   // 只读 ID 用，可设 -1
#define LCD_CS_PIN    24
#define LCD_DC_PIN    25
#define LCD_RST_PIN   44
#define LCD_BL_PIN    -1   // 背光常亮（接 3.3V）；如需 PWM 调光改为某 GPIO

/* ===================== 8. TF 卡 (SPI, 飞利浦 8GB microSD) ===================== */
/* ⚠️ Arduino-ESP32 默认 VSPI 引脚(14/13/12/15)与本板摄像头 10~17 冲突，必须显式指定！
 *   下面为“无冲突占位默认值”，★对照正点原子官方 SD 例程核对后修改。
 *   接线：TF 卡模块 CLK/MOSI/MISO/CS 依次接以下 GPIO；VCC=3.3V（勿接 5V，会烧板）。 */
#define SD_CLK_PIN    47
#define SD_MOSI_PIN   48
#define SD_MISO_PIN   7
#define SD_CS_PIN     2

/* ===================== 9. WiFi / 网络参数（可选「开发遥测」，完全离线运行也可） ===================== */
/* ⚠️ 离线闭环约束（阶段八收口）：设备脱离网络，所有运行与处理都在单片机上形成闭环，
   不依赖任何云端/外网。识别主路径 = 端侧 TFLite Micro；安全底线 = HC-SR04 超声波。
   - ENABLE_WIFI_TELEMETRY=0（默认）：WiFi/HTTP/云端整套编译排除，纯离线运行（省 Flash/电费）。
   - ENABLE_WIFI_TELEMETRY=1：仅作为「开发期遥测/调试」把识别结果异步发云端归档，
     不参与任何引导决策，断网也不影响运行；云端不再是兜底，仅可选旁路。
   08_云端识别服务 已降级为「可选开发遥测」（见 08 README），非运行时依赖。 */
#define ENABLE_WIFI_TELEMETRY  0    // ★ 量产/实际佩戴务必保持 0（纯离线闭环）
#define WIFI_SSID          "YOUR_SSID"        // 仅 ENABLE_WIFI_TELEMETRY=1 时生效（可留空）
#define WIFI_PASSWORD      "YOUR_PASSWORD"
#define CLOUD_API_URL      "http://YOUR_SERVER_IP:5000/api/recognize"
/* 遥测 HTTP 参数（仅 ENABLE_WIFI_TELEMETRY=1 生效）：异步、非阻塞、失败即丢弃，不影响引导 */
#define HTTP_TIMEOUT_MS    3000
#define HTTP_MAX_RETRY     1
#define WIFI_RECONNECT_MS  15000             // 离线时非阻塞重连尝试间隔

/* ===================== 9b. 端侧 AI 识别（YOLO-nano / TFLite Micro）—— 唯一识别主路径 ===================== */
/* ⚠️ 模型权重需你自行训练/转换（见 11_端侧AI识别/model_tools/）。本机无 GPU/无外网，无法代训。
   无模型时系统自动优雅回退：仅走 HC-SR04 超声波安全底线（纯离线，无需任何云端），
   保证现在就能烧录运行；转好模型放进 TF 卡即自动启用端侧 AI，无需改代码。 */
#define MODEL_PATH_SD      "/model/detect.tflite"  // TF 卡上的量化检测模型（放 /model/ 目录）
#define MODEL_MAX_BYTES    (400 * 1024)  // 模型文件上限（量化 YOLO-nano 实测 <300KB）
#define LOCAL_DETECT_W     96     // 检测输入张量宽（建议 96/160，越小越快；盲道/斑马线/井盖够用）
#define LOCAL_DETECT_H     96     // 检测输入张量高
/* 类别顺序必须与训练/转换时一致（见 model_tools/README）：0=盲道 1=斑马线 2=井盖 3=障碍 4=其他/背景 */
#define CLASS_COUNT        5
#define CLASS_BLINDPATH    0
#define CLASS_ZEBRA        1
#define CLASS_MANHOLE      2
#define CLASS_OBSTACLE     3
#define CLASS_OTHER        4
#define DET_CONF_THRESH    0.45f  // 端侧检测置信度阈值（低于则视为“未识别”，由超声波兜底补位）
#define DET_NMS_THRESH     0.45f  // 非极大值抑制 IoU 阈值
#define LOCAL_INFER_INTERVAL_MS 600  // 端侧推理间隔（≈1.6 FPS @96×96；降帧可提帧率）
/* TFLite Micro tensor arena：N8R8 + 量化 YOLO-nano(96×96,5类) 实测 <300KB；512KB 留余量 */
#define TFLM_TENSOR_ARENA   (512 * 1024)
/* 焦距系数（像素）：用于由检测框高度估算障碍距离。需按你的镜头标定，
   公式 dist(m)= FOCAL_PX * OBSTACLE_REAL_H_M / box_h_px，OV2640@96 约 110~150。 */
#define OBSTACLE_FOCAL_PX   130.0f
#define OBSTACLE_REAL_H_M   0.8f   // 典型障碍(人/柱)真实高度(米)，用于距离估算

/* ===================== 10. 采集与运行参数 ===================== */
#define CAPTURE_FPS        1                  // 抽帧率：1 帧/秒
#define IMAGE_WIDTH        320                // QVGA
#define IMAGE_HEIGHT       240
#define JPEG_QUALITY       70                 // 默认 JPEG 压缩质量
#define JPEG_QUALITY_GOOD  80                 // 信号好时
#define JPEG_QUALITY_POOR  40                 // 信号差时

/* ===================== 11. 电池与低功耗 ===================== */
/* ⚠️ 电量采样引脚：锂电池经 TP4056→AMS1117 输出 3.3V，但 BOM 未含电量采样线。
 * 需在电池正极(TP4056 的 BAT 端)与 GND 之间接 2 个等值电阻(如 100k+100k)分压，
 * 中点接以下 ADC 引脚（Vmid = Vbat/2：4.2V→2.1V，落在 3.3V 量程内）。
 * 原代码误用 GPIO3（UART0 调试串口 RX），会破坏串口日志 → 改为 GPIO8（空闲 ADC1，无冲突）。 */
#define BATTERY_ADC_PIN    8
#define BATTERY_DIVIDER    2.0f               // 分压比：实际电压 = ADC 电压 × 2
#define IDLE_POWERDOWN_MS  30000              // 空闲 30s 关闭摄像头
#define STATIC_FPS         0.5                // 静止时降帧

/* ===================== 12. 离线兜底阈值 ===================== */
#define ULTRA_FALLBACK_CM  50                 // 超声波 <50cm 触发强震/蜂鸣

/* ===================== 12b. 天气模式（雨天/湿滑路面预警，物理按钮本地切换） ===================== */
/* 雨天/湿滑路面制动距离变长，障碍预警阈值整体 +1m 更保守（实现见 obstacle_warning.cpp risk_of）。
   设备完全离线、不能靠云端下发天气，故运行时由【物理按钮】本地切换（详见 main.cpp read_weather_button）：
     - 单击拨动开关/按键翻转湿滑档；
     - 开启后：障碍“距离->风险”用湿滑档（<1.5m LV3 / <3m LV2 / <4m LV1），
       同时超声波触发阈值由 50cm 放宽到 80cm（更早发现前方障碍）。
   默认关闭（干燥正常档）。WEATHER_BTN_PIN 接一个带下拉/上拉的 GPIO 按钮/拨动开关。 */
#define WEATHER_MODE_DEFAULT  false
#define WEATHER_BTN_PIN      33    // 天气模式切换按钮（物理，离线可用；BOOT 33 内置上拉）
#define WEATHER_BTN_DEBOUNCE_MS 300  // 按钮去抖窗口

/* ===================== 13. 看门狗 ===================== */
#define WDT_TIMEOUT_MS     30000              // 硬件看门狗 30s

/* ===================== 14. 设备标识 ===================== */
#define DEVICE_ID          "BLIND_GUIDE_001"  // 用于云端日志归档

#endif /* CONFIG_H */
