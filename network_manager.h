/**
 * @file    network_manager.h
 * @brief   WiFi 连接管理 + HTTP POST 上传（带重试）+ 云端 JSON 结果解析
 * @stage   阶段三（网络上传→结果解析）/ 阶段六（弱网重试、降级通知）
 *
 * 依赖：WiFi.h、HTTPClient.h、ArduinoJson.h
 */
#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "protocol.h"

/** @brief 连接/重连 WiFi，返回信号强度 RSSI（dBm） */
int  wifi_connect();

/** @brief 当前 WiFi 是否已连接 */
bool wifi_is_connected();

/**
 * @brief 组装请求 JSON 并 POST 到云端，解析返回结果
 * @param image_base64  02 模块产出的 Base64 图像
 * @param ultra_cm      超声波距离（cm），无则 -1
 * @param battery       电量百分比 0~100
 * @param result        解析后的识别结果（输出）
 * @return true 成功且未降级
 */
bool cloud_recognize(const String &image_base64, float ultra_cm,
                     int battery, RecognizeResult &result);

/**
 * @brief 根据 RSSI 选择 JPEG 质量（阶段六动态压缩）
 */
int select_jpeg_quality(int rssi);

#endif
