/**
 * @file    camera_capture.cpp
 * @brief   OV2640 采集实现
 * @stage   阶段三
 */
#include "camera_capture.h"
#include <esp_camera.h>
#include <Base64.h>

// —— 摄像头引脚：正点原子 DNEP32S3 板载 DVP，固定 GPIO10~GPIO19 ——
// 以下为官方 camera 例程推荐引脚（8 位数据线 + 同步信号），
// 若官方例程有更新，以官方为准替换本数组。
static camera_config_t camera_cfg = {
    .pin_pwdn     = -1,
    .pin_reset    = -1,
    .pin_xclk     = 10,
    .pin_sscb_sda = 11,
    .pin_sscb_scl = 12,
    .pin_d7       = 13,
    .pin_d6       = 14,
    .pin_d5       = 15,
    .pin_d4       = 16,
    .pin_d3       = 17,
    .pin_d2       = 18,
    .pin_d1       = 19,
    .pin_d0       = 19,   // DVP 低 8 位中多余线可复用，具体以板载走线为准
    .pin_vsync    = 12,
    .pin_href     = 13,
    .pin_pclk     = 14,
    .xclk_freq_hz = 20000000,
    .ledc_timer   = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size   = FRAMESIZE_QVGA,   // 320x240
    .jpeg_quality = JPEG_QUALITY,
    .fb_count     = 2,
    .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
};

bool camera_init() {
    esp_err_t err = esp_camera_init(&camera_cfg);
    if (err != ESP_OK) {
        Serial.printf("[CAM] init failed: 0x%x\n", err);
        return false;
    }
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_vflip(s, 1);      // 摄像头朝下安装，需垂直翻转
        s->set_hmirror(s, 1);
    }
    Serial.println("[CAM] init OK");
    return true;
}

bool camera_capture_base64(int quality, String &out) {
    sensor_t *s = esp_camera_sensor_get();
    if (s) s->set_quality(s, quality);   // 动态压缩质量（阶段六）

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("[CAM] frame buffer get failed");
        return false;
    }
    // JPEG 已硬件编码，直接 Base64
    size_t b64_len = Base64.encodedLength(fb->len);
    char *b64 = (char *)ps_malloc(b64_len + 1);
    if (!b64) {
        esp_camera_fb_return(fb);
        return false;
    }
    Base64.encode(b64, (char *)fb->buf, fb->len);
    out = String(b64);
    free(b64);
    esp_camera_fb_return(fb);
    return true;
}

void camera_power_down() {
    // 进入低功耗：关闭帧缓冲与时钟（阶段六）
    esp_camera_deinit();
    Serial.println("[CAM] powered down");
}

bool camera_power_up() {
    return camera_init();
}
