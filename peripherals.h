/**
 * @file    peripherals.h
 * @brief   全部外设驱动统一接口：震动马达 / 灯带 / 蜂鸣器 / 超声波 / I2S 功放
 * @stage   阶段二（硬件连接）/ 阶段三（外设驱动开发）
 *
 * 引脚见 config.h：
 *   DRV2605L(I2C 38/39) | 蜂鸣器(GPIO40) | WS2812B(GPIO41)
 *   HC-SR04(Trig42/Echo43) | MAX98357A I2S(BCLK4/LRCLK5/DIN6)
 */
#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <Arduino.h>
#include "config.h"

/* ---------- 初始化 ---------- */
void peripherals_init();

/* ---------- 震动马达 DRV2605L（I2C） ---------- */
void vibration_set(uint8_t pwm);          // 0~100 占空比
void vibration_stop();

/* ---------- WS2812B 灯带 ---------- */
void led_set_color(uint32_t rgb);        // 0xRRGGBB，全灯统一颜色
void led_blink(uint32_t rgb, int times); // 闪烁（阶段五 3 级红色闪烁）
void led_off();

/* ---------- 有源蜂鸣器 ---------- */
void buzzer_on();
void buzzer_off();
void buzzer_beep(int ms);                 // 间歇蜂鸣（阶段五 2 级）

/* ---------- HC-SR04 超声波 ---------- */
float ultrasonic_read_cm();               // 返回距离 cm（离线兜底用）

/* ---------- MAX98357A I2S 功放 ---------- */
void audio_init();
void audio_play_pcm(const int16_t *data, size_t samples); // 播放 PCM
void audio_play_wav(const char *path);   // 播放 TF 卡 WAV（16k/16bit/单声道）
void audio_say(const String &text);       // 语音播报（命中预录 WAV 经 I2S 出声，否则蜂鸣）

/* ---------- 电池电量（TP4056 分压采样，0~100） ---------- */
int battery_read_pct();

#endif
