/**
 * @file    peripherals.cpp
 * @brief   外设驱动实现
 * @stage   阶段二 / 阶段三
 */
#include "peripherals.h"
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <driver/i2s.h>
#include <SD.h>
#include <string.h>

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
    // 电池电压采样：TP4056 后经 2:1 分压进入 ADC，11dB 衰减适配 0~3.3V 量程
    pinMode(BATTERY_ADC_PIN, INPUT);
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    audio_init();
}

/* ---------- 电池电量（实际运行需真实读数，原先恒为 100%） ---------- */
int battery_read_pct() {
    // 分压比 2:1 → 实际电压 = adc电压 * 2；锂电池 3.0V(截止)~4.2V(满)
    int raw = analogRead(BATTERY_ADC_PIN);
    float v = (raw / 4095.0f) * 3.3f * 2.0f;
    if (v <= 3.0f) return 0;
    if (v >= 4.2f) return 100;
    return (int)((v - 3.0f) / (4.2f - 3.0f) * 100.0f);
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

// 关键词 → TF 卡上预录语音文件（16kHz / 16bit / 单声道 WAV，放在 /voice/ 目录）
static const char *voice_file_for(const String &text) {
    if (text.indexOf("左") >= 0)   return "/voice/left.wav";
    if (text.indexOf("右") >= 0)   return "/voice/right.wav";
    if (text.indexOf("转弯") >= 0) return "/voice/turn.wav";
    if (text.indexOf("终止") >= 0 || text.indexOf("结束") >= 0) return "/voice/end.wav";
    if (text.indexOf("障碍") >= 0) return "/voice/obs.wav";
    if (text.indexOf("安全") >= 0 || text.indexOf("正常") >= 0) return "/voice/safe.wav";
    return nullptr;
}

// 播放 TF 卡上的 16-bit PCM WAV（须与 I2S 配置 16kHz/16bit/单声道一致）
void audio_play_wav(const char *path) {
    File f = SD.open(path);
    if (!f) return;
    uint8_t hdr[44];
    if (f.read(hdr, sizeof(hdr)) != sizeof(hdr)) { f.close(); return; }
    if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) { f.close(); return; }
    // 定位 "data" 子块（标准 44 字节头即在此；否则扫描一次）
    uint32_t dataSize = 0;
    if (memcmp(hdr + 36, "data", 4) == 0) {
        memcpy(&dataSize, hdr + 40, 4);
    } else {
        f.seek(12);
        uint8_t c[8];
        while (f.read(c, 8) == 8) {
            uint32_t sz; memcpy(&sz, c + 4, 4);
            if (memcmp(c, "data", 4) == 0) { dataSize = sz; break; }
            f.seek(f.position() + sz);
        }
    }
    int16_t buf[512];
    uint32_t left = dataSize;
    while (left > 0 && f.available()) {
        size_t want = min((uint32_t)sizeof(buf), left);
        size_t rd = f.read((uint8_t *)buf, want);
        if (rd == 0) break;
        size_t written;
        i2s_write(I2S_NUM_0, buf, rd, &written, portMAX_DELAY);
        left -= rd;
    }
    f.close();
}

// 语音播报：命中预录文件则经 I2S（MAX98357A）真实出声；否则退化为蜂鸣。
// 增加 1s 节流，避免离线 10Hz 循环把同一句重复播 10 次/秒。
void audio_say(const String &text) {
    if (text.length() == 0) return;
    static unsigned long last_ms = 0;
    unsigned long now = millis();
    if (now - last_ms < 1000) return;     // 1 秒内最多播一次
    last_ms = now;

    const char *vf = voice_file_for(text);
    if (vf && SD.exists(vf)) audio_play_wav(vf);
    else buzzer_beep(120);                 // 无对应语音文件时的退化提示
}
