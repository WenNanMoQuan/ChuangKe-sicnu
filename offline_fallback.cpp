/**
 * @file    offline_fallback.cpp
 * @brief   离线超声波安全底线 + 模式定义
 * @stage   阶段五（离线超声波兜底）/ 阶段八收口（离线闭环）
 *
 * ⚠️ 离线闭环约束：设备脱离网络，全部处理在单片机闭环。本模块不再依赖 WiFi，
 * HC-SR04 作为「端侧 TFLite 识别」之外的纯安全底线——无论有网无网、有无模型，
 * 只要前方有近距离障碍就一定触发强震+蜂鸣。check_mode() 在离线闭环下恒为
 * MODE_OFFLINE（不再有“在线/离线”切换概念，识别主路径本就在端侧）。
 */
#include "core_logic.h"
#include "peripherals.h"   // ultrasonic_read_cm / buzzer / vibration

RunMode check_mode(RunMode cur) {
    // 离线闭环：不存在“在线模式”，识别主路径已经全程端侧；恒为离线档。
    return MODE_OFFLINE;
}

/**
 * @brief 超声波安全底线扫描（10Hz 限流）。
 *  - 端侧识别成功时主循环也会调用本函数，与视觉结果“融合”（两者任一命中即报警），
 *    实现“视觉+超声”双保险；端侧无模型/失败时则作为唯一兜底（来源 SRC_ULTRA）。
 *  - 天气模式（雨天/湿滑）下触发阈值由 50cm 放宽到 80cm，更早预警。
 */
FeedbackCmd offline_fallback() {
    FeedbackCmd fb; // 默认无反馈
    static unsigned long last_scan = 0;
    static unsigned long last_voice_ms = 0;
    unsigned long now = millis();
    if (now - last_scan < 100) return fb; // 限到 10Hz
    last_scan = now;

    float d = ultrasonic_read_cm();
    // 天气模式（雨天/湿滑）下放宽到 80cm，更早发现前方障碍
    float thr = g_weather_mode ? 80.0f : (float)ULTRA_FALLBACK_CM;
    if (d > 0 && d < thr) {
        fb.vibration_pwm = 90;
        fb.buzzer_on = true;
        fb.led_color = 0xFF0000;
        // 语音 1s 最小间隔，避免 10Hz 扫描时刷屏（修复前每次命中都播报）
        if (now - last_voice_ms > 1000) {
            fb.voice_text = "注意障碍";
            last_voice_ms = now;
        }
    }
    return fb;
}
