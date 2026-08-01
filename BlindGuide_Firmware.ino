/**
 * @file    main.cpp
 * @brief   盲道智能引导系统 —— 端侧主固件（总装入口）
 * @stage   阶段三（主循环闭环）/ 阶段六（看门狗、日志、低功耗）
 *
 * 主循环链路：拍照 → 编码 → 上传 → 等待结果 → 解析 → 执行反馈
 *            → LCD 更新 → TF 卡记录日志 → （BLE 推送状态）
 */
#include "config.h"
#include "protocol.h"
#include "camera_capture.h"
#include "network_manager.h"
#include "peripherals.h"
#include "display_ui.h"
#include "core_logic.h"
#include <SD.h>
#include <SPI.h>
#include <esp_task_wdt.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

static BLECharacteristic *pStatusChar;
static unsigned long last_active_ms = 0;
static bool camera_on = true;
static float battery_pct = 100.0f;

/* ---------- TF 卡日志（阶段六：运行记录） ---------- */
void log_to_sd(const String &line) {
    File f = SD.open("/runlog.csv", FILE_APPEND);
    if (f) { f.print(line); f.close(); }
}

/* ---------- BLE 状态推送（阶段三：手机调试 App） ---------- */
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

void setup() {
    Serial.begin(115200);
    peripherals_init();              // 外设（含 I2C/Wire、灯带、蜂鸣、I2S）
    camera_init();                   // 摄像头
    display_init();                  // LCD
    wifi_connect();                  // WiFi
    ble_init();                      // BLE

    // —— 天气模式（雨天/湿滑预警，设计 GAP 已闭环）——
    // 默认取 config.h 的 WEATHER_MODE_DEFAULT；运行时由云端天气字段自动切换（见 loop）。
    set_weather_mode(WEATHER_MODE_DEFAULT);

    // TF 卡：显式指定 SPI 引脚（默认 VSPI 14/13/12/15 与本板摄像头 10~17 冲突）
    if (!SD.begin(SD_CS_PIN, SD_CLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, 4000000))
        Serial.println("[SD] mount failed"); // TF 卡

    // 硬件看门狗 30s（阶段六）
    esp_task_wdt_init(WDT_TIMEOUT_MS / 1000, true);
    esp_task_wdt_add(NULL);

    log_to_sd("ts,status,obstacles,battery,rssi\n");
    last_active_ms = millis();
}

void loop() {
    esp_task_wdt_reset();            // 喂狗

    bool connected = wifi_is_connected();          // 快速状态查询，绝不阻塞
    int rssi = connected ? WiFi.RSSI() : -100;

    // —— 离线模式：高速超声波兜底 + 非阻塞周期重连（修复前每帧阻塞~10s） ——
    if (!connected) {
        static unsigned long last_scan = 0;
        static unsigned long last_reconn = 0;
        unsigned long now = millis();
        if (now - last_scan >= 100) {              // 10Hz 超声波扫描
            last_scan = now;
            FeedbackCmd fb = offline_fallback();    // 内部已含 1s 语音节流
            vibration_set(fb.vibration_pwm);
            led_set_color(fb.led_color);
            fb.buzzer_on ? buzzer_on() : buzzer_off();
            if (fb.voice_text.length()) audio_say(fb.voice_text);
            display_update(rssi, (int)battery_pct, "OFFLINE", "ultrasonic", 0);
            pStatusChar->setValue("OFFLINE"); pStatusChar->notify();
            log_to_sd(String(now) + ",OFFLINE,ultrasonic," +
                      String((int)battery_pct) + "," + String(rssi) + "\n");
        }
        if (now - last_reconn >= WIFI_RECONNECT_MS) {   // 每 15s 发起一次非阻塞重连
            last_reconn = now;
            wifi_reconnect_async();
        }
        delay(5);
        return;
    }

    // —— 在线模式：1 FPS 完整链路 ——
    int quality = select_jpeg_quality(rssi);

    // 电量每 10s 真实读一次（修复前恒为 100）
    static unsigned long last_battery_ms = 0;
    if (millis() - last_battery_ms > 10000) {
        battery_pct = battery_read_pct();
        last_battery_ms = millis();
    }

    // —— 低功耗：空闲 30s 关摄像头（阶段六） ——
    if (camera_on && (millis() - last_active_ms > IDLE_POWERDOWN_MS)) {
        camera_power_down(); camera_on = false;
    }

    String b64;
    RecognizeResult result;
    bool got = false;

    if (camera_on) {
        if (camera_capture_base64(quality, b64)) {
            got = cloud_recognize(b64, ultrasonic_read_cm(), (int)battery_pct, result);
        }
    }

    // —— 天气模式：按云端天气字段自动切换（带变更去抖，仅状态变化时才切换，避免每帧抖动）——
    if (got) {
        bool want_wet = weather_is_wet(result.weather);
        if (want_wet != get_weather_mode()) {
            set_weather_mode(want_wet);
            Serial.printf("[WX] weather=%d -> wet=%d\n", result.weather, want_wet);
        }
    }

    // —— 决策：在线用 06 核心逻辑，离线/降级用超声波兜底 ——
    FeedbackCmd final_fb;
    String mode_str, res_str;
    if (got && !result.degraded) {
        BlindPathStatus st = guidance_update(result);
        FeedbackCmd pf = guidance_to_feedback(st);
        FeedbackCmd of = obstacle_to_feedback(result);
        final_fb = schedule_feedback(pf, of);
        mode_str = "ONLINE";
        res_str = "BP=" + String(st) + " OBS=" + String(result.obstacle_cnt)
                  + " WX=" + (get_weather_mode() ? "WET" : "DRY");
        last_active_ms = millis();
    } else {
        final_fb = offline_fallback();   // 离线/降级兜底
        mode_str = result.degraded ? "DEGRADED" : "OFFLINE";
        res_str = "ultrasonic fallback";
    }

    // —— 执行反馈（交给 04 外设） ——
    vibration_set(final_fb.vibration_pwm);
    led_set_color(final_fb.led_color);
    final_fb.buzzer_on ? buzzer_on() : buzzer_off();
    audio_say(final_fb.voice_text);

    // —— LCD 更新（05） + BLE 推送 + TF 日志（阶段六） ——
    display_update(rssi, (int)battery_pct, mode_str.c_str(), res_str.c_str(), 0);
    pStatusChar->setValue(res_str.c_str());
    pStatusChar->notify();
    log_to_sd(String(millis()) + "," + mode_str + "," +
              String(result.obstacle_cnt) + "," + String((int)battery_pct) +
              "," + String(rssi) + "\n");

    delay(1000 / CAPTURE_FPS);       // 1 FPS 抽帧
}
