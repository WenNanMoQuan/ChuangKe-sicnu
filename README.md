# 01 数据协议与引脚配置模块

> 对应规划：阶段一（端云接口协议规范）/ 阶段二（引脚分配方案）

## 一、功能（本模块做什么）
- `config.h`：集中定义全部硬件引脚分配、网络参数、采集/低功耗/看门狗等运行常量，是整套固件的“总开关”。
- `protocol.h`：定义端云 JSON 交互协议的字段常量、枚举（盲道 5 态、障碍 3 级）和解析后的 C++ 结构体（`RecognizeResult` / `FeedbackCmd` / `Obstacle`），供所有模块共享，避免字段拼写不一致。

## 二、作用（在系统中的位置）
- 这是**地基模块**，被 02~07 全部嵌入式模块 `#include`。
- 任何硬件接线变更、WiFi 地址变更、参数调优，只改这里，不用动业务代码。
- `protocol.h` 同时是**端云双方的契约**：云端服务（08）返回的 JSON 必须严格匹配这里的字段名，设备端才能正确解析。

## 三、拼接方法（如何接入主工程）
1. 将本文件夹整体放入 Arduino 工程根目录（与 `main.cpp` 同级）。
2. 在 `main.cpp` 及各驱动 `.cpp` 顶部加入：
   ```cpp
   #include "config.h"
   #include "protocol.h"
   ```
3. 上线前务必修改 `config.h` 中的 `WIFI_SSID`、`WIFI_PASSWORD`、`CLOUD_API_URL`、`DEVICE_ID`。
4. 摄像头、LCD 引脚以正点原子官方例程为准（本文件已预留 `CAMERA_USE_BOARD_DEFAULT_PINS` / `LCD_USE_BOARD_DEFAULT_PINS` 宏），替换官方例程的引脚数组即可。
5. 云端（08_云端识别服务）返回的 JSON 字段名必须与 `protocol.h` 的 `K_*` 宏一致，否则解析为空。
