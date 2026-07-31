# 07 主循环固件（总装入口）

> 对应规划：阶段三（主循环逻辑、基础闭环、BLE、TF 卡日志）/ 阶段六（看门狗、低功耗、日志）

## 一、功能
`main.cpp` 是端侧固件的 **唯一入口（Arduino `setup()`/`loop()`）**，把 02~06 全部模块串成完整闭环：
```
拍照(02) → Base64编码 → 上传解析(03) → 核心决策(06) → 执行反馈(04) → LCD(05) → TF日志 → BLE推送
```
并集成：硬件看门狗（30s 自动重启）、空闲 30s 关摄像头低功耗、TF 卡 CSV 运行日志、BLE 向手机调试 App 推送状态。

## 二、作用
- **总装层**：定义了阶段三要求的“采图→编码→上传→等待结果→解析→执行反馈→LCD 更新→TF 卡记录日志”主循环。
- 是“端侧基础闭环运行”的交付物（阶段三），也是后续阶段五/六功能迭代的承载文件。

## 三、拼接方法
1. 将本 `main.cpp` 放在 Arduino 工程根目录，与 `01~06` 各模块文件夹并列（或把它们的 `.h` 加入 include 路径、`*.cpp` 加入编译）。
2. 依赖库（在库管理器 / `platformio.ini` 中添加）：
   `esp_camera`、`Base64`、`WiFi`、`HTTPClient`、`ArduinoJson`、`Adafruit_NeoPixel`、
   `lvgl`、`SD(ESP32)`、`BLEDevice(NimBLE)`、`driver/i2s`、`esp_task_wdt`。
3. 编译前在 `config.h` 填好 WiFi / 云端地址 / DEVICE_ID；摄像头、LCD 引脚以正点原子官方例程为准。
4. 板型选择：正点原子 DNEP32S3（ESP32-S3，8MB PSRAM）。烧录后通过串口与 nRF Connect 验证 BLE，串口看主循环日志。
5. 阶段五/六新增的业务逻辑直接扩充 `loop()` 中“决策”段落，调用 06 模块即可，主框架不动。
