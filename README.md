# 03 网络通信模块

> 对应规划：阶段三（WiFi 连接管理、HTTP POST 上传、云端 JSON 结果解析）/ 阶段六（弱网重试、动态压缩）

## 一、功能
- `wifi_connect()`：自动连接、断线重连、返回 RSSI 信号强度。
- `cloud_recognize()`：组装请求 JSON（device_id + image_base64 + ultrasonic_cm + battery + ts），POST 到 `CLOUD_API_URL`，失败自动重试（最多 3 次，超时 5s），并解析云端返回的 `RecognizeResult`。
- `select_jpeg_quality()`：依据 RSSI 动态选择 JPEG 质量（信号好 80 / 差 40）。

## 二、作用
- 端侧数据链路中段：“编码→上传→等待结果→解析”。
- 把 02 模块的 Base64 图像和超声波/电量打包发给 08 云端服务，并把结构化结果交回主循环，供 06 业务逻辑使用。
- 解析严格依赖 `protocol.h` 的 `K_*` 字段宏，与云端契约对齐。

## 三、拼接方法
1. `#include "network_manager.h"`，依赖 `WiFi.h`、`HTTPClient.h`、`ArduinoJson.h`。
2. 主循环顺序：
   ```cpp
   int rssi = wifi_connect();
   int q = select_jpeg_quality(rssi);          // 阶段六动态压缩
   camera_capture_base64(q, b64);              // 来自 02
   bool ok = cloud_recognize(b64, ultra, bat, result); // 本模块
   if (!ok || result.degraded) { /* 走 06 离线兜底 */ }
   ```
3. 断网时 `cloud_recognize` 返回 false，主循环据此切换 `06_核心业务逻辑模块` 的离线超声波模式。
4. `CLOUD_API_URL`、`WIFI_*` 在 `config.h` 配置。
