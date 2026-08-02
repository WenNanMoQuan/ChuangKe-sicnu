/**
 * @file    camera_capture.h
 * @brief   OV2640 摄像头采集 —— 双模式：
 *           1) 检测模式：RGB565 小图（LOCAL_DETECT_W×H），供端侧 TFLite Micro 推理；
 *           2) 云端模式：JPEG 硬件编码 + Base64，供云端兜底/遥测上传。
 * @stage   阶段三 / 阶段八（端侧 AI 双流）
 *
 * 依赖：esp_camera（ESP32 官方组件）、Base64（内置 base64_util.h）
 * 引脚：正点原子 DNEP32S3 板载 DVP —— 数据 Y0~Y7=GPIO10~17，PCLK=18，
 *       VSYNC=19，HREF=20，SCCB(I2C)=38/39，XCLK=9（详见 camera_capture.cpp）。
 *
 * 设计：默认以「检测模式」常驻（输出 RGB565 小图给端侧 AI，零外网依赖）；
 *       仅在需要云端兜底/遥测时，临时切到 JPEG 抓一帧编码上传，用完切回。
 *       避免每帧都 JPEG 全分辨率编码（省 CPU/带宽，端侧推理为主）。
 */
#ifndef CAMERA_CAPTURE_H
#define CAMERA_CAPTURE_H

#include <Arduino.h>
#include "config.h"

/**
 * @brief  初始化摄像头（默认检测模式 RGB565）
 * @return true 成功
 */
bool camera_init();

/**
 * @brief  以 RGB565 抓一帧供端侧 AI 推理（不编码、不传网）。
 *         同时给出原图宽高（OV2640 实际输出尺寸，与检测张量尺寸可能不同）。
 * @param  out     输出缓冲区（调用方分配 >= w*h*2 字节），RGB565 行优先
 * @param  w/h     输出实际帧宽高
 * @return true 成功
 * @note   若 out 为 NULL，仅偷看一帧并丢弃（用于低功耗唤醒后探帧）。
 */
bool camera_capture_rgb565(uint8_t *out, size_t &w, size_t &h);

/**
 * @brief  以 JPEG 抓一帧并编码为 Base64（云端兜底/遥测时使用）。
 * @param  quality  JPEG 质量 0~100（由网络模块动态传入）
 * @param  out      输出的 Base64 字符串
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
