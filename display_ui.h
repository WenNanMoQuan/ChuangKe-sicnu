/**
 * @file    display_ui.h
 * @brief   2.4 寸 LCD 状态显示（LVGL）
 * @stage   阶段二（LCD 验证）/ 阶段三（LVGL 界面）/ 阶段六（完善状态显示）
 *
 * 显示内容：WiFi 信号、电池电量%、当前工作模式、最近识别结果摘要、TF 卡剩余空间
 * 引脚见 config.h（LCD_USE_BOARD_DEFAULT_PINS，按正点原子官方 SPI 接线）
 */
#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <Arduino.h>
#include "protocol.h"

/** @brief 初始化 LVGL 与屏幕 */
void display_init();

/** @brief 刷新一帧状态（阶段六：实时显示） */
void display_update(int rssi, int battery_pct, const char *mode,
                    const char *last_result, int sd_free_kb);

#endif
