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
    if (!SD.begin()) Serial.println("[SD] mount failed"); // TF 卡

    // 硬件看门狗 30s（阶段六）
    esp_task_wdt_init(WDT_TIMEOUT_MS / 1000, true);
    esp_task_wdt_add(NULL);

    log_to_sd("ts,status,obstacles,battery,rssi\n");
    last_active_ms = millis();
}

void loop() {
    esp_task_wdt_reset();            // 喂狗

    int rssi = wifi_connect();
    int quality = select_jpeg_quality(rssi);

    // —— 低功耗：空闲 30s 关摄像头（阶段六） ——
    if (camera_on && (millis() - last_active_ms > IDLE_POWERDOWN_MS)) {
        camera_power_down(); camera_on = false;
    }

    String b64;
    RecognizeResult result;
    bool got = false;
    RunMode mode = check_mode(MODE_ONLINE);

    if (mode == MODE_ONLINE && camera_on) {
        if (camera_capture_base64(quality, b64)) {
            got = cloud_recognize(b64, ultrasonic_read_cm(), (int)battery_pct, result);
        }
    }

    // —— 决策：在线用 06 核心逻辑，离线用超声波兜底 ——
    FeedbackCmd final_fb;
    String mode_str, res_str;
    if (mode == MODE_ONLINE && got && !result.degraded) {
        BlindPathStatus st = guidance_update(result);
        FeedbackCmd pf = guidance_to_feedback(st);
        FeedbackCmd of = obstacle_to_feedback(result);
        final_fb = schedule_feedback(pf, of);
        mode_str = "ONLINE";
        res_str = "BP=" + String(st) + " OBS=" + String(result.obstacle_cnt);
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
