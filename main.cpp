/**
 * @file    main.cpp
 * @brief   盲道智能引导系统 —— 端侧主固件（总装入口，离线闭环）
 * @stage   阶段八收口：脱离网络，全部处理在单片机闭环
 *
 * 离线闭环运行链路（无任何云端/外网依赖）：
 *   1) 端侧识别（唯一识别主路径，全程离线）：
 *        OV2640(RGB565) → 11 端侧 AI(TFLite Micro) → RecognizeResult(SRC_LOCAL)
 *   2) 超声波安全底线（融合 + 兜底）：
 *        HC-SR04 10Hz → offline_fallback() → 与视觉结果融合；
 *        端侧无模型/未识别到目标时，作为唯一兜底来源 SRC_ULTRA。
 *   → 06 核心逻辑(guidance + obstacle + 天气模式) → 04 外设反馈
 *   → 05 LCD + BLE 推送 + TF 卡日志 + 看门狗
 *
 * 可选旁路（不参与引导决策）：若 ENABLE_WIFI_TELEMETRY=1，仅把识别结果异步发云端
 * 做开发期遥测归档；失败即丢弃，断网也不影响运行。
 */
#include "config.h"
#include "protocol.h"
#include "camera_capture.h"
#include "peripherals.h"
#include "display_ui.h"
#include "core_logic.h"
#include "ondevice_detect.h"       // 11 端侧 AI
#include <SD.h>
#include <SPI.h>
#include <esp_task_wdt.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#if ENABLE_WIFI_TELEMETRY
  #include "network_manager.h"
  #include <WiFi.h>
#endif

static BLECharacteristic *pStatusChar;
static unsigned long last_active_ms = 0;
static bool camera_on = true;
static float battery_pct = 100.0f;

/* ---------- TF 卡日志 ---------- */
void log_to_sd(const String &line) {
    File f = SD.open("/runlog.csv", FILE_APPEND);
    if (f) { f.print(line); f.close(); }
}

/* ---------- BLE 状态推送 ---------- */
class StatusCb : public BLEServerCallbacks {
    void onConnect(BLEServer*) { Serial.println("[BLE] connected"); }
    void onDisconnect(BLEServer*) { Serial.println("[BLE] disconnected"); }
};
void ble_init() {
    BLEDevice::init("BlindGuide");
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new StatusCb());
    BLEService *pSvc = pServer->createService("1234");
    pStatusChar = pSvc->createCharacteristic("5678",
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    pStatusChar->addDescriptor(new BLE2902());
    pSvc->start();
    BLEDevice::getAdvertising()->start();
}

/* ---------- 天气模式物理按钮（离线本地切换，不依赖云端） ---------- */
static bool   g_weather_btn_last = HIGH;   // WEATHER_BTN_PIN 内置上拉：未按=HIGH
static unsigned long g_weather_btn_ms = 0;
void read_weather_button() {
    // 单击翻转雨天/湿滑档；带去抖窗口，避免抖动误触发。
    bool cur = digitalRead(WEATHER_BTN_PIN);
    if (cur == LOW && g_weather_btn_last == HIGH) {           // 下降沿 = 按下
        unsigned long now = millis();
        if (now - g_weather_btn_ms > WEATHER_BTN_DEBOUNCE_MS) {
            bool want = !get_weather_mode();
            set_weather_mode(want);
            g_weather_btn_ms = now;
            Serial.printf("[WX] button toggle -> wet=%d\n", want);
            display_update(0, (int)battery_pct, "WX_TOGGLE", want ? "WET" : "DRY", 0);
        }
    }
    g_weather_btn_last = cur;
}

#if ENABLE_WIFI_TELEMETRY
/* ---------- 可选开发遥测（旁路，不参与引导） ---------- */
#endif

void setup() {
    Serial.begin(115200);
    peripherals_init();              // 外设（I2C/Wire、灯带、蜂鸣、I2S）
    camera_init();                   // 摄像头（默认 RGB565 检测模式）
    display_init();                  // LCD

    // 天气模式：物理按钮本地切换（默认干燥档，见 config.h WEATHER_MODE_DEFAULT）
    pinMode(WEATHER_BTN_PIN, INPUT_PULLUP);
    set_weather_mode(WEATHER_MODE_DEFAULT);

    // 端侧 AI 模型加载（无模型则自动走超声波安全底线，纯离线也能烧录运行）
    local_detect_init();
    local_detect_print_status();

    // TF 卡：显式指定 SPI 引脚（默认 VSPI 与摄像头冲突）
    if (!SD.begin(SD_CS_PIN, SD_CLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, 4000000))
        Serial.println("[SD] mount failed");

#if ENABLE_WIFI_TELEMETRY
    wifi_connect();                  // 仅开发遥测用，失败不影响离线运行
#else
    Serial.println("[NET] WiFi 遥测已关闭（纯离线闭环运行）");
#endif

    ble_init();                      // BLE（手机可看状态/电量，不参与引导）

    // 硬件看门狗 30s
    esp_task_wdt_init(WDT_TIMEOUT_MS / 1000, true);
    esp_task_wdt_add(NULL);

    log_to_sd("ts,status,source,obstacles,battery\n");
    last_active_ms = millis();
}

void loop() {
    esp_task_wdt_reset();            // 喂狗

    // 天气按钮（离线本地切换雨天/湿滑档）
    read_weather_button();

    // 电量每 10s 真实读一次
    static unsigned long last_battery_ms = 0;
    if (millis() - last_battery_ms > 10000) {
        battery_pct = battery_read_pct();
        last_battery_ms = millis();
    }

    // 低功耗：空闲 30s 关摄像头
    if (camera_on && (millis() - last_active_ms > IDLE_POWERDOWN_MS)) {
        camera_power_down(); camera_on = false;
    }

    FeedbackCmd final_fb;
    String mode_str, res_str;

    // —— 端侧识别（唯一主路径，全程离线，按 LOCAL_INFER_INTERVAL_MS 节流） ——
    static unsigned long last_infer_ms = 0;
    bool do_infer = (millis() - last_infer_ms >= LOCAL_INFER_INTERVAL_MS);

    RecognizeResult result;
    bool got_local = false;

    if (camera_on && local_detect_ready() && do_infer) {
        static uint8_t *g_rgb = nullptr;
        if (!g_rgb) g_rgb = (uint8_t*)malloc(320 * 240 * 2);
        size_t w = 0, h = 0;
        if (g_rgb && camera_capture_rgb565(g_rgb, w, h)) {
            if (local_detect_run(g_rgb, w, h, result)) {
                result.source = SRC_LOCAL;
                got_local = true;
                last_infer_ms = millis();
                last_active_ms = millis();   // 视觉活跃，延缓低功耗关摄像头
            }
        }
    }

    // —— 超声波安全底线（融合 + 兜底，10Hz） ——
    // 与视觉结果融合：视觉命中或超声命中，任一报警即执行；超声同时作为无模型/失败兜底。
    FeedbackCmd ultra_fb = offline_fallback();

    if (got_local) {
        BlindPathStatus st = guidance_update(result);
        FeedbackCmd pf = guidance_to_feedback(st);
        FeedbackCmd of = obstacle_to_feedback(result);
        FeedbackCmd path_obs = schedule_feedback(pf, of);
        // 视觉 + 超声 融合（任一更高强度即采用，保证双保险）
        final_fb.vibration_pwm = max(path_obs.vibration_pwm, ultra_fb.vibration_pwm);
        final_fb.led_color     = (ultra_fb.vibration_pwm > path_obs.vibration_pwm)
                                 ? ultra_fb.led_color : path_obs.led_color;
        final_fb.buzzer_on     = path_obs.buzzer_on || ultra_fb.buzzer_on;
        final_fb.voice_text    = path_obs.voice_text.length() ? path_obs.voice_text : ultra_fb.voice_text;
        mode_str = "LOCAL";
        res_str = "BP=" + String(st) + " OBS=" + String(result.obstacle_cnt)
                  + " ZB=" + (result.zebra_detected ? "1" : "0")
                  + " MH=" + (result.manhole_detected ? "1" : "0")
                  + " WX=" + (get_weather_mode() ? "WET" : "DRY");
    } else {
        // 端侧无模型或本周期未推理：超声波作为唯一安全底线（来源 SRC_ULTRA）
        final_fb = ultra_fb;
        mode_str = local_detect_ready() ? "LOCAL_IDLE" : "ULTRA";
        res_str = "ultrasonic safety baseline";
    }

    // —— 执行反馈 ——
    vibration_set(final_fb.vibration_pwm);
    led_set_color(final_fb.led_color);
    final_fb.buzzer_on ? buzzer_on() : buzzer_off();
    audio_say(final_fb.voice_text);

    // —— LCD + BLE + TF 日志 ——
    display_update(0, (int)battery_pct, mode_str.c_str(), res_str.c_str(), 0);
    pStatusChar->setValue(res_str.c_str());
    pStatusChar->notify();
    log_to_sd(String(millis()) + "," + mode_str + "," +
              (got_local ? "LOCAL" : "ULTRA") + "," +
              String(result.obstacle_cnt) + "," + String((int)battery_pct) + "\n");

#if ENABLE_WIFI_TELEMETRY
    // 可选开发遥测：异步把识别摘要发云端归档（失败即丢弃，不影响引导）
    static unsigned long last_tele = 0;
    unsigned long now = millis();
    if (wifi_is_connected() && (now - last_tele >= 5000)) {
        last_tele = now;
        // 仅发轻量摘要（不含图像），不参与任何决策
        String b64 = "";  // 离线闭环：不回传图像，仅状态遥测
        RecognizeResult tele; tele.source = got_local ? SRC_LOCAL : SRC_ULTRA;
        tele.obstacle_cnt = result.obstacle_cnt;
        cloud_recognize(b64, ultrasonic_read_cm(), (int)battery_pct, tele);
    }
#endif

    delay(1000 / CAPTURE_FPS);       // 1 FPS 抽帧（端侧识别在其内部节流到 ~1.6Hz）
}
