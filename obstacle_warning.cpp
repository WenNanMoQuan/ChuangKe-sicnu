/**
 * @file    obstacle_warning.cpp
 * @brief   障碍物三级分级预警 + 播报优先级调度
 * @stage   阶段五
 *
 * 三级风险（正常档，距离阈值）：
 *  1 级（2~3 米）：轻震 PWM30% + 黄色灯带
 *  2 级（1~2 米）：中震 PWM60% + 间歇蜂鸣 + 橙色灯带
 *  3 级（0~1 米）：强震 PWM90% + 持续蜂鸣 + 红色闪烁灯带
 * 调度：3 级障碍优先于盲道引导；同类型反馈最小间隔 1s 防重复播报。
 *
 * 天气模式（雨天/湿滑，设计 GAP 已闭环）：制动距离变长，距离阈值整体 +1m 更保守
 *  （<1.5m LV3 / <3m LV2 / <4m LV1），由 risk_of() + g_weather_mode 控制。
 */
#include "core_logic.h"

/* 天气模式全局状态（编译期默认见 config.h WEATHER_MODE_DEFAULT） */
bool g_weather_mode = WEATHER_MODE_DEFAULT;

void set_weather_mode(bool on) { g_weather_mode = on; }
bool get_weather_mode()        { return g_weather_mode; }

/* 距离(m) -> 风险等级，受天气模式影响：
 *  正常： <1 LV3 / <2 LV2 / <3 LV1 / 其他 无
 *  湿滑： <1.5 LV3 / <3 LV2 / <4 LV1 / 其他 无（阈值整体 +1m 更保守） */
int risk_of(float dist) {
    if (g_weather_mode) {
        if (dist < 1.5f) return RISK_LV3;
        if (dist < 3.0f) return RISK_LV2;
        if (dist < 4.0f) return RISK_LV1;
        return RISK_NONE;
    }
    if (dist < 1.0f) return RISK_LV3;
    if (dist < 2.0f) return RISK_LV2;
    if (dist < 3.0f) return RISK_LV1;
    return RISK_NONE;
}

FeedbackCmd obstacle_to_feedback(const RecognizeResult &res) {
    FeedbackCmd fb; // 默认全 0 = 无威胁
    RiskLevel top = RISK_NONE;
    for (int i = 0; i < res.obstacle_cnt; i++) {
        const Obstacle &o = res.obstacles[i];
        // 以“距离”为物理真值重算风险（不盲信云端 risk_level，云在雨天也会误判），
        // 取 distance 换算与云端 risk_level 的较大者，更稳健；并自动套用天气模式。
        RiskLevel by_dist = (RiskLevel)risk_of(o.distance);
        RiskLevel r = (o.risk_level > by_dist) ? o.risk_level : by_dist;
        if (r > top) top = r;
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

    // 3 级障碍优先：震动/灯带取“更强”者。
    // 灯色：只要存在障碍风险（obs_fb.vibration_pwm>0 即 risk>=LV1）就以障碍灯色为准，
    // 避免 LV1/LV2 障碍的橙色/黄色被盲道绿色覆盖（修复前仅 vib>=90 才用障碍色）。
    out.vibration_pwm = max(path_fb.vibration_pwm, obs_fb.vibration_pwm);
    out.led_color     = (obs_fb.vibration_pwm > 0) ? obs_fb.led_color : path_fb.led_color;
    out.buzzer_on     = path_fb.buzzer_on || obs_fb.buzzer_on;

    // 语音：优先障碍语音；同类型最小间隔 1s 防重复
    unsigned long now = millis();
    String chosen = obs_fb.voice_text.length() ? obs_fb.voice_text : path_fb.voice_text;
    if (chosen.length() && (now - last_voice_ms > 1000)) {
        out.voice_text = chosen; last_voice_ms = now;
    }
    return out;
}
