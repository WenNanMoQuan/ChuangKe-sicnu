/**
 * @file    obstacle_warning.cpp
 * @brief   障碍物三级分级预警 + 播报优先级调度
 * @stage   阶段五
 *
 * 三级风险：
 *  1 级（2~3 米）：轻震 PWM30% + 黄色灯带
 *  2 级（1~2 米）：中震 PWM60% + 间歇蜂鸣 + 橙色灯带
 *  3 级（0~1 米）：强震 PWM90% + 持续蜂鸣 + 红色闪烁灯带
 * 调度：3 级障碍优先于盲道引导；同类型反馈最小间隔 1s 防重复播报。
 */
#include "core_logic.h"

FeedbackCmd obstacle_to_feedback(const RecognizeResult &res) {
    FeedbackCmd fb; // 默认全 0 = 无威胁
    RiskLevel top = RISK_NONE;
    for (int i = 0; i < res.obstacle_cnt; i++) {
        if (res.obstacles[i].risk_level > top) top = res.obstacles[i].risk_level;
    }
    switch (top) {
        case RISK_LV1: fb.vibration_pwm = 30; fb.led_color = 0xFFFF00; fb.voice_text = "注意前方障碍"; break;
        case RISK_LV2: fb.vibration_pwm = 60; fb.led_color = 0xFFA500; fb.buzzer_on = true; fb.voice_text = "前方障碍较近"; break;
        case RISK_LV3: fb.vibration_pwm = 90; fb.led_color = 0xFF0000; fb.buzzer_on = true; fb.voice_text = "紧急障碍"; break;
        default: break;
    }
    return fb;
}

FeedbackCmd schedule_feedback(const FeedbackCmd &path_fb, const FeedbackCmd &obs_fb) {
    static unsigned long last_voice_ms = 0;
    FeedbackCmd out;

    // 3 级障碍优先：震动/灯带取“更强”者
    out.vibration_pwm = max(path_fb.vibration_pwm, obs_fb.vibration_pwm);
    out.led_color     = (obs_fb.vibration_pwm >= 90) ? obs_fb.led_color : path_fb.led_color;
    out.buzzer_on     = path_fb.buzzer_on || obs_fb.buzzer_on;

    // 语音：优先障碍语音；同类型最小间隔 1s 防重复
    unsigned long now = millis();
    String chosen = obs_fb.voice_text.length() ? obs_fb.voice_text : path_fb.voice_text;
    if (chosen.length() && (now - last_voice_ms > 1000)) {
        out.voice_text = chosen; last_voice_ms = now;
    }
    return out;
}
