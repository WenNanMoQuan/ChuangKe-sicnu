/**
 * @file    camera_capture.h
 * @brief   OV2640 摄像头采集 + JPEG 硬件编码 + Base64 编码
 * @stage   阶段三：嵌入式基础软件开发（图像采集→编码压缩→网络上传）
 *
 * 依赖：esp_camera（ESP32 官方组件）、Base64（Arduino 库）
 * 引脚：正点原子 DNEP32S3 板载 DVP 固定 GPIO10~GPIO19，以官方 camera 例程为准。
 */
#ifndef CAMERA_CAPTURE_H
#define CAMERA_CAPTURE_H

#include <Arduino.h>
#include "config.h"

/**
 * @brief  初始化摄像头
 * @return true 成功
 */
bool camera_init();

/**
 * @brief  拍摄一帧并编码为 Base64 字符串
 * @param  quality  JPEG 质量 0~100（由网络模块动态传入）
 * @param  out      输出的 Base64 字符串（调用方负责释放）
 * @return true 成功
 */
bool camera_capture_base64(int quality, String &out);

/**
 * @brief  关闭摄像头电源（阶段六低功耗：空闲 30s 进入低功耗）
 */
void camera_power_down();

/**
 * @brief  重新上电并初始化摄像头
 */
bool camera_power_up();

#endif
