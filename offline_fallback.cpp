/**
 * @file    offline_fallback.cpp
 * @brief   离线超声波兜底 + 在线/离线模式切换
 * @stage   阶段五（离线超声波兜底）/ 阶段六
 *
 * WiFi 断连时自动启用 HC-SR04 以 10Hz 检测前方障碍物，
 * 距离 < 50cm 触发蜂鸣和强震；网络恢复后自动切回在线模式。
 */
#include "core_logic.h"
#include "peripherals.h"   // ultrasonic_read_cm / buzzer / vibration
#include <WiFi.h>

RunMode check_mode(RunMode cur) {
    bool connected = (WiFi.status() == WL_CONNECTED);
    if (connected) return MODE_ONLINE;   // 网络恢复 -> 回在线
    return MODE_OFFLINE;                 // 断网 -> 离线兜底
}

FeedbackCmd offline_fallback() {
    FeedbackCmd fb; // 默认无反馈
    static unsigned long last_scan = 0;
    unsigned long now = millis();
    if (now - last_scan < 100) return fb; // 限到 10Hz
    last_scan = now;

    float d = ultrasonic_read_cm();
    if (d > 0 && d < ULTRA_FALLBACK_CM) {
        fb.vibration_pwm = 90;
        fb.buzzer_on = true;
        fb.led_color = 0xFF0000;
        fb.voice_text = "注意障碍";
    }
    return fb;
}
