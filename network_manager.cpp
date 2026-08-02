/**
 * @file    network_manager.cpp
 * @brief   WiFi + HTTP 上传 + JSON 解析实现
 * @stage   阶段三 / 阶段六
 */
#include "network_manager.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

int wifi_connect() {
    if (WiFi.status() == WL_CONNECTED) return WiFi.RSSI();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 20) {
        delay(500); tries++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WIFI] connected, RSSI=%d\n", WiFi.RSSI());
        return WiFi.RSSI();
    }
    Serial.println("[WIFI] connect failed");
    return -100;
}

bool wifi_is_connected() {
    return WiFi.status() == WL_CONNECTED;
}

int select_jpeg_quality(int rssi) {
    // 信号好（>-60）用高质量，差（<-75）用低质量减小体积
    if (rssi > -60) return JPEG_QUALITY_GOOD;
    if (rssi < -75) return JPEG_QUALITY_POOR;
    return JPEG_QUALITY;
}

void wifi_reconnect_async() {
    // 仅发起连接，不阻塞等待（后台自动完成）。主循环按 WIFI_RECONNECT_MS 周期调用，
    // 避免离线时每帧阻塞 10s 导致超声波兜底响应卡顿、看门狗风险。
    if (!wifi_is_connected()) {
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
}

bool cloud_recognize(const String &image_base64, float ultra_cm,
                     int battery, RecognizeResult &result) {
    // 离线时直接返回，不在此处阻塞重试（重连由主循环异步处理）
    if (!wifi_is_connected()) return false;

    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.begin(CLOUD_API_URL);
    http.addHeader("Content-Type", "application/json");

    // —— 组装请求体（协议见 protocol.h） ——
    StaticJsonDocument<512> req;
    req[K_DEVICE_ID] = DEVICE_ID;
    req[K_IMAGE]     = image_base64;
    req[K_ULTRA]     = ultra_cm;
    req[K_BATTERY]   = battery;
    req[K_TS]        = millis();
    String reqStr;
    serializeJson(req, reqStr);

    // —— 失败重试（最多 HTTP_MAX_RETRY 次） ——
    int httpCode = -1;
    for (int i = 0; i < HTTP_MAX_RETRY; i++) {
        httpCode = http.POST(reqStr);
        if (httpCode == HTTP_CODE_OK) break;
        delay(300);
    }
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[NET] POST failed, code=%d\n", httpCode);
        http.end();
        return false;
    }

    // —— 解析返回 JSON ——
    String payload = http.getString();
    http.end();
    DynamicJsonDocument resp(4096);
    if (deserializeJson(resp, payload) != DeserializationError::Ok) {
        Serial.println("[NET] JSON parse error");
        return false;
    }
    result.degraded = resp[K_DEGRADED] | false;

    // —— 天气字段（仅可选开发遥测归档用；离线闭环中天气由物理按钮本地切换，不在此设置）——
    const char *wx = resp[K_WEATHER] | "clear";
    if      (!strcmp(wx, "rain"))  result.weather = WX_RAIN;
    else if (!strcmp(wx, "snow"))  result.weather = WX_SNOW;
    else if (!strcmp(wx, "fog"))   result.weather = WX_FOG;
    else if (!strcmp(wx, "clear")) result.weather = WX_CLEAR;
    else                           result.weather = WX_UNKNOWN;

    JsonObject bp = resp[K_BLIND_PATH];
    if (!bp.isNull()) {
        result.path_detected = bp[K_DETECTED] | false;
        result.offset_dir    = bp[K_OFFSET_DIR] | "center";
        result.offset_angle  = bp[K_OFFSET_ANGLE] | 0.0;
        const char *st = bp[K_STATUS] | "FOLLOWING";
        if      (!strcmp(st, "OFFSETLEFT"))  result.path_status = BP_OFFSET_LEFT;
        else if (!strcmp(st, "OFFSETRIGHT")) result.path_status = BP_OFFSET_RIGHT;
        else if (!strcmp(st, "TURNING"))     result.path_status = BP_TURNING;
        else if (!strcmp(st, "ENDDETECTED")) result.path_status = BP_END_DETECTED;
        else                                 result.path_status = BP_FOLLOWING;
    }

    JsonArray obs = resp[K_OBSTACLES];
    result.obstacle_cnt = 0;
    if (!obs.isNull()) {
        for (JsonObject o : obs) {
            if (result.obstacle_cnt >= 4) break;
            Obstacle &ob = result.obstacles[result.obstacle_cnt++];
            ob.type      = o[K_TYPE] | "";
            ob.distance  = o[K_DISTANCE] | 0.0;
            ob.direction = o[K_DIRECTION] | "center";
            int rl = o[K_RISK] | 0;
            ob.risk_level = (RiskLevel)rl;
        }
    }

    JsonObject fb = resp[K_FEEDBACK];
    if (!fb.isNull()) {
        result.feedback.vibration_pwm = fb[K_VIBRATION] | 0;
        result.feedback.buzzer_on     = fb[K_BUZZER] | false;
        result.feedback.led_color     = (uint32_t)(fb[K_LED_COLOR] | 0);
        result.feedback.voice_text    = fb[K_VOICE] | "";
    }
    return true;
}
