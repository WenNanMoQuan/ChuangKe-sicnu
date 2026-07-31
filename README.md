# 06 核心业务逻辑模块

> 对应规划：阶段五（核心功能联调落地）/ 阶段六（离线兜底）

## 一、功能
本模块是端侧“大脑”，把云端识别结果转化为交互决策，含三个子块：

1. **盲道引导状态机**（`guidance_fsm.cpp`）
   - 5 态：正常跟随 / 左偏 / 右偏 / 转弯 / 盲道终止。
   - **三帧滑动窗口防抖**：连续 3 帧中 ≥2 帧相同才切换状态，过滤偶发误识别。
   - `guidance_to_feedback()` 把状态映射为震动强度 + 语音 + 灯色。

2. **障碍物三级分级预警**（`obstacle_warning.cpp`）
   - 1 级(2~3m)：轻震30% + 黄灯；2 级(1~2m)：中震60% + 间歇蜂鸣 + 橙灯；3 级(0~1m)：强震90% + 持续蜂鸣 + 红灯闪。
   - **播报优先级调度**：3 级障碍优先于盲道提示；同类型语音最小间隔 1s 防重复。

3. **离线超声波兜底**（`offline_fallback.cpp`）
   - 断网自动切离线：`check_mode()` 判定；离线时 10Hz 超声波检测，<50cm 触发强震+蜂鸣；网络恢复自动回在线。

## 二、作用
- 处于端侧数据链路末端：“解析 → 执行反馈”的决策层，直接驱动 04 外设。
- 实现阶段五两大核心业务（盲道引导、障碍分级预警）和断网兜底，是“基础可用”的关键。

## 三、拼接方法
1. `#include "core_logic.h"`，并在工程中加入本文件夹三个 `.cpp` 及 `04_外设驱动模块`。
2. 主循环典型调用顺序：
   ```cpp
   RunMode mode = check_mode(g_mode);
   FeedbackCmd final_fb;
   if (mode == MODE_ONLINE) {
       BlindPathStatus st = guidance_update(result);          // 防抖状态
       FeedbackCmd pf = guidance_to_feedback(st);
       FeedbackCmd of = obstacle_to_feedback(result);
       final_fb = schedule_feedback(pf, of);                  // 优先级调度
   } else {
       final_fb = offline_fallback();                        // 离线兜底
   }
   // 执行反馈（交给 04）
   vibration_set(final_fb.vibration_pwm);
   led_set_color(final_fb.led_color);
   final_fb.buzzer_on ? buzzer_on() : buzzer_off();
   audio_say(final_fb.voice_text);
   ```
3. 防抖窗口大小（3）、调度最小间隔（1s）、兜底阈值（50cm）均为可调常量，集中维护。
