# 05 LCD 状态显示模块

> 对应规划：阶段二（LCD 显示验证）/ 阶段三（LVGL 界面开发）/ 阶段六（完善状态显示）

## 一、功能
- 基于 LVGL 在 2.4 寸 LCD 上显示：WiFi 信号强度、电池电量%、当前工作模式、最近识别结果摘要、TF 卡剩余空间。
- `display_init()` 初始化屏幕；`display_update()` 每轮主循环刷新一次。

## 二、作用
- 端侧“人机状态可视层”。让用户/调试者能直接看到设备运行状态，是阶段二验证“LCD 可显示基础信息”和阶段六“完善状态显示”的落地代码。

## 三、拼接方法
1. `#include "display_ui.h"`，依赖 `lvgl` 及正点原子官方 LCD 的 LVGL 移植层（`lv_port_disp_init`）。
2. 主循环：`display_init()` 一次；每轮调用
   ```cpp
   display_update(rssi, battery_pct, mode_str, result_str, sd_free_kb);
   ```
3. `lv_timer_handler()` 需在 `loop()` 中周期性调用（已内置在 `display_update` 内）。
4. LCD 的具体 SPI 引脚以正点原子官方 2.4 寸 LCD 例程为准（本模块预留 `LCD_USE_BOARD_DEFAULT_PINS`）。
