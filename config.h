/**
 * @file    config.h
 * @brief   盲道智能引导系统 —— 全局硬件引脚分配与运行参数配置
 * @stage   阶段二：硬件原型搭建 / 阶段三：嵌入式基础软件开发
 *
 * 说明：
 *  - 引脚分配严格依据《阶段规划_优化版》中“引脚分配方案”一节。
 *  - DVP 摄像头接口由正点原子 DNEP32S3 开发板硬件固定占用 GPIO10~GPIO19，
 *    此处摄像头具体引脚以官方 camera 例程为准（见 camera_capture 模块注释）。
 *  - 其余外设引脚为本项目自定义分配，修改后需同步更新接线原理图。
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/* ===================== 1. 摄像头 (OV2640, DVP) ===================== */
/* 正点原子 DNEP32S3 板载 DVP 接口固定占用 GPIO10~GPIO19。
   实际 8 位数据线 + VSYNC/HREF/PCLK/XCLK/SIOD/SIOC 映射请直接复用
   官方 camera 例程中的 pins，不要在此随意改动，否则无法取图。 */
#define CAMERA_USE_BOARD_DEFAULT_PINS   true

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
#define ULTRA_TRIG_PIN     42
#define ULTRA_ECHO_PIN     2

/* ===================== 6. MAX98357A I2S 功放 (三线) ===================== */
#define I2S_BCLK_PIN       4
#define I2S_LRCLK_PIN      5
#define I2S_DIN_PIN        6

/* ===================== 7. LCD (SPI, 2.4寸, 正点原子官方接线) ===================== */
/* 具体 SPI 引脚请参照正点原子 2.4 寸 LCD 模块官方接线图，
   通常包含 SCK/MOSI/DC/RST/CS，此处预留宏供 display 模块引用。 */
#define LCD_USE_BOARD_DEFAULT_PINS  true

/* ===================== 8. TF 卡 (SPI, 飞利浦 8GB) ===================== */
/* TF 卡使用 ESP32-S3 默认 SPI 接口，由 SD 库自动管理，无需单独定义引脚。 */

/* ===================== 9. WiFi / 网络参数 ===================== */
#define WIFI_SSID          "YOUR_SSID"        // 设备上线前替换为实际 WiFi
#define WIFI_PASSWORD      "YOUR_PASSWORD"
#define CLOUD_API_URL      "http://YOUR_SERVER_IP:5000/api/recognize"
#define HTTP_TIMEOUT_MS    5000               // 单次请求超时
#define HTTP_MAX_RETRY     3                  // 上传失败最大重试次数

/* ===================== 10. 采集与运行参数 ===================== */
#define CAPTURE_FPS        1                  // 抽帧率：1 帧/秒
#define IMAGE_WIDTH        320                // QVGA
#define IMAGE_HEIGHT       240
#define JPEG_QUALITY       70                 // 默认 JPEG 压缩质量
#define JPEG_QUALITY_GOOD  80                 // 信号好时
#define JPEG_QUALITY_POOR  40                 // 信号差时

/* ===================== 11. 电池与低功耗 ===================== */
#define BATTERY_ADC_PIN    3                  // 锂电池电压采样 (TP4056)
#define IDLE_POWERDOWN_MS  30000              // 空闲 30s 关闭摄像头
#define STATIC_FPS         0.5                // 静止时降帧

/* ===================== 12. 离线兜底阈值 ===================== */
#define ULTRA_FALLBACK_CM  50                 // 超声波 <50cm 触发强震/蜂鸣

/* ===================== 13. 看门狗 ===================== */
#define WDT_TIMEOUT_MS     30000              // 硬件看门狗 30s

/* ===================== 14. 设备标识 ===================== */
#define DEVICE_ID          "BLIND_GUIDE_001"  // 用于云端日志归档

#endif /* CONFIG_H */
