/**
 * @file    guidance_fsm.cpp
 * @brief   盲道引导状态机 + 三帧滑动窗口防抖
 * @stage   阶段五
 *
 * 状态（5 态）：FOLLOWING / OFFSETLEFT / OFFSETRIGHT / TURNING / ENDDETECTED
 * 防抖：连续 3 帧中有 2 帧以上判定为相同状态才切换并触发反馈，
 *       过滤光线突变、阴影遮挡导致的偶发误识别。
 */
#include "core_logic.h"

static BlindPathStatus g_current = BP_FOLLOWING;
static BlindPathStatus g_window[3] = {BP_FOLLOWING, BP_FOLLOWING, BP_FOLLOWING};
static int g_idx = 0;

BlindPathStatus guidance_update(const RecognizeResult &res) {
    // 取云端盲道状态作为本帧候选
    BlindPathStatus cand = res.path_status;
    if (!res.path_detected) cand = BP_FOLLOWING; // 未检测到盲道默认跟随

    g_window[g_idx] = cand;
    g_idx = (g_idx + 1) % 3;

    // 统计窗口内出现最多的状态
    int cnt[5] = {0};
    for (int i = 0; i < 3; i++) cnt[g_window[i]]++;
    BlindPathStatus best = g_current; int maxc = 0;
    for (int s = 0; s < 5; s++) {
        if (cnt[s] > maxc) { maxc = cnt[s]; best = (BlindPathStatus)s; }
    }
    // 三帧中 >=2 帧相同才切换
    if (maxc >= 2 && best != g_current) {
        g_current = best;
    }
    return g_current;
}

BlindPathStatus guidance_current() { return g_current; }

FeedbackCmd guidance_to_feedback(BlindPathStatus st) {
    FeedbackCmd fb;
    switch (st) {
        case BP_FOLLOWING:    fb.vibration_pwm = 20; fb.voice_text = "正常跟随"; fb.led_color = 0x00FF00; break;
        case BP_OFFSET_LEFT:  fb.vibration_pwm = 50; fb.voice_text = "向左修正"; fb.led_color = 0xFFFF00; break;
        case BP_OFFSET_RIGHT: fb.vibration_pwm = 50; fb.voice_text = "向右修正"; fb.led_color = 0xFFFF00; break;
        case BP_TURNING:      fb.vibration_pwm = 70; fb.voice_text = "前方转弯"; fb.led_color = 0x00FFFF; break;
        case BP_END_DETECTED: fb.vibration_pwm = 90; fb.voice_text = "盲道终止"; fb.led_color = 0xFF0000; fb.buzzer_on = true; break;
    }
    return fb;
}
