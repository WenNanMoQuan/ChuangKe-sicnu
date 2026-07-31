/**
 * @file    peripherals.cpp
 * @brief   外设驱动实现
 * @stage   阶段二 / 阶段三
 */
#include "peripherals.h"
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <driver/i2s.h>

static Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

/* ---------- DRV2605L 简易 I2C 写 ---------- */
static void drv_write(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(DRV2605_I2C_ADDR);
    Wire.write(reg); Wire.write(val);
    Wire.endTransmission();
}
static void drv_init() {
    drv_write(0x01, 0x00); // MODE = internal trigger
    drv_write(0x1A, 0x05); // 默认库 5
    drv_write(0x1D, 0x80); // 退出待机
}

void peripherals_init() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    drv_init();
    strip.begin(); strip.show();
    pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
    pinMode(ULTRA_TRIG_PIN, OUTPUT);
    pinMode(ULTRA_ECHO_PIN, INPUT);
    audio_init();
}

/* ---------- 震动 ---------- */
void vibration_set(uint8_t pwm) {
    // 用 DRV2605 的 RMS 增益近似表达强度（0x00~0xFF 映射到 0~100）
    uint8_t g = (uint8_t)(pwm * 255 / 100);
    drv_write(0x1B, 0x00);  // 关闭自动校准
    drv_write(0x1C, g);     // 设置增益
    drv_write(0x1A, 0x80);  // 退出待机
    drv_write(0x0C, 0x01);  // GO 触发
}
void vibration_stop() { drv_write(0x0C, 0x00); }

/* ---------- 灯带 ---------- */
void led_set_color(uint32_t rgb) {
    for (int i = 0; i < LED_COUNT; i++) strip.setPixelColor(i, rgb);
    strip.show();
}
void led_blink(uint32_t rgb, int times) {
    for (int t = 0; t < times; t++) {
        led_set_color(rgb); delay(150); led_off(); delay(150);
    }
}
void led_off() { strip.clear(); strip.show(); }

/* ---------- 蜂鸣器 ---------- */
void buzzer_on()  { digitalWrite(BUZZER_PIN, HIGH); }
void buzzer_off() { digitalWrite(BUZZER_PIN, LOW); }
void buzzer_beep(int ms) { buzzer_on(); delay(ms); buzzer_off(); }

/* ---------- 超声波 ---------- */
float ultrasonic_read_cm() {
    digitalWrite(ULTRA_TRIG_PIN, LOW); delayMicroseconds(2);
    digitalWrite(ULTRA_TRIG_PIN, HIGH); delayMicroseconds(10);
    digitalWrite(ULTRA_TRIG_PIN, LOW);
    long dur = pulseIn(ULTRA_ECHO_PIN, HIGH, 30000);
    return dur * 0.0343 / 2.0;
}

/* ---------- I2S 功放 ---------- */
void audio_init() {
    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = 0,
        .dma_buf_count = 8, .dma_buf_len = 64,
        .use_apll = false
    };
    i2s_pin_config_t pins = {
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num  = I2S_LRCLK_PIN,
        .data_out_num = I2S_DIN_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pins);
}
void audio_play_pcm(const int16_t *data, size_t samples) {
    size_t written;
    i2s_write(I2S_NUM_0, data, samples * 2, &written, portMAX_DELAY);
}
// 简易语音：命中预录关键词则播放对应片段，否则蜂鸣提示（真实 TTS 可由云端返回音频或离线模型补充）
void audio_say(const String &text) {
    if (text.length() == 0) return;
    // TODO: 接入离线 TTS 或云端 TTS 音频流；此处以蜂鸣表示“有语音”
    buzzer_beep(120);
}
