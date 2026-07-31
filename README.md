# 02 摄像头采集模块

> 对应规划：阶段三（图像采集→编码压缩）

## 一、功能
- 初始化 OV2640 摄像头（DVP 接口，板载 GPIO10~GPIO19）。
- 以 1 FPS 抽帧，输出 **QVGA(320×240) JPEG** 图像，并做 **Base64 编码** 供 HTTP 上传。
- 支持动态 JPEG 质量（阶段六：信号好=80，信号差=40）。
- 支持低功耗关断/重启（阶段六：空闲 30s 关摄像头）。

## 二、作用
- 位于端侧数据链路最前端：“拍照”环节。产出 `image_base64` 字段，是云端识别的唯一图像来源。
- 被 `07_主循环固件` 调用，输出喂给 `03_网络通信模块`。

## 三、拼接方法
1. 拷贝 `camera_capture.h/.cpp` 到工程，并 `#include "camera_capture.h"`。
2. 在 `main.cpp` 启动时调用 `camera_init()`。
3. 主循环按 `CAPTURE_FPS` 调用 `camera_capture_base64(quality, out)`，得到的 `out` 作为 `03` 模块的 `image_base64` 输入。
4. 引脚数组以正点原子官方 camera 例程为准；如官方更新，替换 `camera_cfg` 即可。
5. 依赖库：`esp_camera`（ESP32 组件）、`Base64`（Arduino 库），在 `platformio.ini` / Arduino 库管理中添加。
