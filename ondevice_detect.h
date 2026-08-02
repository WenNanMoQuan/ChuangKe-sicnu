/**
 * @file    ondevice_detect.h
 * @brief   端侧 AI 识别封装（TFLite Micro + 后处理）
 * @stage   阶段八（端侧 YOLO-nano 检测，混合架构主路径）
 *
 * 职责：
 *   1) 从 TF 卡加载量化检测模型（detect.tflite），无模型则优雅回退；
 *   2) 把 OV2640 的 RGB565 帧双线性下采样到 LOCAL_DETECT_W×H 输入张量（uint8）；
 *   3) 跑 TFLite Micro 推理；
 *   4) 解码检测输出 → RecognizeResult（盲道偏移/斑马线/井盖/障碍距离+风险）；
 *   5) 统一交给 06 核心逻辑（guidance + obstacle）做反馈决策。
 *
 * ⚠️ 模型权重需你自行训练/转换（见 model_tools/）。本机无 GPU/无外网，无法代训。
 *    无模型时 system_status() 返回 false，主循环自动走「云端兜底或超声波离线」。
 *
 * 依赖：本模块在 PlatformIO 下使用 tensorFlowLite 库（见 platformio.ini）。
 *       为兼容“无 TFLM”的 ArduinoIDE 轻量环境，所有 TFLM 符号放在 .cpp 内，头文件不暴露，
 *       使仅烧录主循环（用云端/离线）时也能编译。真正用到 TFLM 的 .cpp 才链接。
 */
#ifndef ONDEVICE_DETECT_H
#define ONDEVICE_DETECT_H

#include <Arduino.h>
#include "config.h"
#include "protocol.h"

/**
 * @brief  初始化端侧检测：尝试从 MODEL_PATH_SD 加载模型
 * @return true = 模型就绪（可端侧推理）；false = 无模型（走兜底）
 */
bool local_detect_init();

/**
 * @brief  端侧检测是否已就绪（模型成功加载）
 */
bool local_detect_ready();

/**
 * @brief  对一帧 RGB565 图像做端侧检测，写入 RecognizeResult
 * @param  rgb      RGB565 原图（行优先，len = src_w*src_h*2）
 * @param  src_w/h  原图宽高
 * @param  out      输出识别结果（内部填充 source=SRC_LOCAL / 各类检测 / 障碍风险）
 * @return true 成功推理（无论是否检测到目标）；false 表示系统不可用（应兜底）
 * @note   调用前请确认 local_detect_ready()；若未就绪，本函数直接返回 false。
 */
bool local_detect_run(const uint8_t *rgb, size_t src_w, size_t src_h,
                      RecognizeResult &out);

/**
 * @brief  调试：打印端侧检测状态
 */
void local_detect_print_status();

#endif
