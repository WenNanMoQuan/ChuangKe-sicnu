/**
 * @file    core_logic.h
 * @brief   核心业务逻辑的对外接口集合
 * @stage   阶段五（核心功能联调落地）/ 阶段六（离线兜底）
 *
 * 包含：
 *  1) 盲道引导状态机（5 态）+ 三帧滑动窗口防抖
 *  2) 障碍物三级分级预警 + 播报优先级调度
 *  3) 离线超声波兜底（断网时）
 */
#ifndef CORE_LOGIC_H
#define CORE_LOGIC_H

#include <Arduino.h>
#include "config.h"
#include "protocol.h"

/* =================== 1. 盲道引导状态机 =================== */
/** @brief 输入一帧识别结果，返回“已防抖稳定”的盲道状态 */
BlindPathStatus guidance_update(const RecognizeResult &res);

/** @brief 当前稳定状态 */
BlindPathStatus guidance_current();

/** @brief 把盲道状态转换为反馈（震动模式 + 语音提示） */
FeedbackCmd guidance_to_feedback(BlindPathStatus st);

/* =================== 2. 障碍物分级预警 =================== */
/** @brief 由障碍列表计算最高风险等级反馈（三级映射） */
FeedbackCmd obstacle_to_feedback(const RecognizeResult &res);

/** @brief 播报优先级调度：3 级障碍优先于盲道提示，同类型最小间隔 1s */
FeedbackCmd schedule_feedback(const FeedbackCmd &path_fb, const FeedbackCmd &obs_fb);

/* =================== 2b. 天气模式（雨天/湿滑预警，设计 GAP 闭环） =================== */
/** @brief 当前天气模式：true=湿滑更保守档（阈值整体 +1m），false=正常档 */
extern bool g_weather_mode;
/** @brief 运行时切换天气模式（离线闭环：由物理按钮 read_weather_button 调用） */
void  set_weather_mode(bool on);
/** @brief 读取当前天气模式 */
bool  get_weather_mode();
/** @brief 距离(m)->风险等级，受天气模式影响（正常：<1 LV3/<2 LV2/<3 LV1；湿滑：<1.5 LV3/<3 LV2/<4 LV1） */
int   risk_of(float dist);

/* =================== 3. 离线超声波兜底 =================== */
/** @brief 在线/离线模式 */
enum RunMode { MODE_ONLINE, MODE_OFFLINE };

/** @brief 检测是否需切换离线（断网）或回在线（网络恢复） */
RunMode check_mode(RunMode cur);

/** @brief 离线模式：10Hz 超声波检测，<50cm 触发强震+蜂鸣 */
FeedbackCmd offline_fallback();

#endif
