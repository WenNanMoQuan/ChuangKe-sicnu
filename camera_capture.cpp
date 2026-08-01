/**
 * @file    camera_capture.cpp
 * @brief   OV2640 采集实现
 * @stage   阶段三
 */
#include "camera_capture.h"
#include <esp_camera.h>
#include "base64_util.h"   // 内置 Base64，避免外部库依赖

// —— 摄像头引脚：正点原子 DNEP32S3 板载 DVP ——
// ⚠️ 警告：ESP32-S3 的摄像头引脚高度依赖具体板型，必须与你手里的
//    正点原子 DNEP32S3 官方 OV2640 例程完全一致，否则取图会失败/花屏。
//    下面按《项目硬件模块清单.xlsx》“引脚分配速查”填写（数据 Y0~Y7=GPIO10~17，
//    控制 PCLK/VSYNC/HREF=GPIO18/19/20，SCCB=I2C 38/39）。这些引脚互相不冲突，
//    但 XCLK(9) 与板载走线请以官方例程最终覆盖为准。
static camera_config_t camera_cfg = {
    .pin_pwdn     = -1,
    .pin_reset    = -1,
    .pin_xclk     = 9,     // XCLK/MCLK：以官方例程为准（常见为 GPIO9/GPIO15）
    .pin_sscb_sda = 38,    // 与 DRV2605L 共用 I2C 总线（xlsx）
    .pin_sscb_scl = 39,
    .pin_d7       = 10,    // 数据总线 Y0~Y7
    .pin_d6       = 11,
    .pin_d5       = 12,
    .pin_d4       = 13,
    .pin_d3       = 14,
    .pin_d2       = 15,
    .pin_d1       = 16,
    .pin_d0       = 17,
    .pin_vsync    = 19,    // 控制线（xlsx：VSYNC=19）
    .pin_href     = 20,    // 控制线（xlsx：HREF=20）
    .pin_pclk     = 18,    // 控制线（xlsx：PCLK=18）
    .xclk_freq_hz = 20000000,
    .ledc_timer   = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size   = FRAMESIZE_QVGA,   // 320x240
    .jpeg_quality = JPEG_QUALITY,
    .fb_count     = 2,
    .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
    .fb_location  = CAMERA_FB_IN_PSRAM,   // 帧缓冲放入 8MB PSRAM（充分利用板载 PSRAM）
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
    // JPEG 已硬件编码，直接 Base64（内置实现，无外部库依赖）
    out = base64_encode(fb->buf, fb->len);
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
