/* lv_conf.h — LVGL 8.3 配置（放在 sketch 目录，Arduino 编译时自动包含）
 * 与 display_ui.cpp 的要求一致：LV_COLOR_DEPTH=16、LV_MEM_SIZE>=64KB、开启自定义心跳。 */
#if 1
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_HOR_RES_MAX          320
#define LV_VER_RES_MAX          240

#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        0

/* 显存：64KB。本板 8MB PSRAM，可在 ArduinoIDE 工具->PSRAM 选 "Enabled(OPI)"，
 * 并在下方把 LV_MEM_ADR 改为 (uint32_t)0x3FA00000（PSRAM 基址）以用 PSRAM 承载显存。 */
#define LV_MEM_SIZE             (64 * 1024)
#define LV_MEM_ADR              0   /* 0 = 由 LVGL 自动分配（内部 SRAM） */

#define LV_TICK_CUSTOM          1
#define LV_TICK_CUSTOM_INCLUDE  <Arduino.h>
#define LV_TICK_CUSTOM_SYS_TIME_EXPR  (millis())

#define LV_USE_LOG              0
#define LV_USE_USER_DATA        1
#define LV_USE_PERF_MONITOR     0

#endif /* LV_CONF_H */
#endif /* #if 1 */
