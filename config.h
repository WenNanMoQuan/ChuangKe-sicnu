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

/* ===================== 9. WiFi / 网络参数 ===================== */
#define WIFI_SSID          "YOUR_SSID"        // 设备上线前替换为实际 WiFi
#define WIFI_PASSWORD      "YOUR_PASSWORD"
#define CLOUD_API_URL      "http://YOUR_SERVER_IP:5000/api/recognize"
/* 端到端延迟验收 ≤3s：单次请求超时 3s，最多重试 1 次（最坏 ≈3s+开销）。
   说明：规划文档原写“5s/重试3次”，但其最坏 15s 与“≤3s 延迟”验收指标冲突，
   此处按“实际运行优化”改为 3s/重试1次，确保满足验收且仍保留一次容错。 */
#define HTTP_TIMEOUT_MS    3000
#define HTTP_MAX_RETRY     1
#define WIFI_RECONNECT_MS  15000             // 离线时非阻塞重连尝试间隔（避免每帧阻塞）

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

/* ===================== 12b. 天气模式（雨天/湿滑路面预警，设计 GAP 已闭环） ===================== */
/* 雨天/湿滑路面制动距离变长，障碍预警阈值整体 +1m 更保守（实现见 obstacle_warning.cpp risk_of）。
   编译期默认关闭；运行时可调用 set_weather_mode(true) 开启（建议由云端按天气下发，
   或接一个物理拨动开关/按键）。开启后：
     - 在线：障碍“距离->风险”用湿滑档（<1.5m LV3 / <3m LV2 / <4m LV1）
     - 离线：超声波触发阈值由 50cm 放宽到 80cm（更早发现前方障碍） */
#define WEATHER_MODE_DEFAULT  false

/* ===================== 13. 看门狗 ===================== */
#define WDT_TIMEOUT_MS     30000              // 硬件看门狗 30s

/* ===================== 14. 设备标识 ===================== */
#define DEVICE_ID          "BLIND_GUIDE_001"  // 用于云端日志归档

#endif /* CONFIG_H */
