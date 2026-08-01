/**
 * @file    display_ui.cpp
 * @brief   2.4 寸 LCD 状态显示（LovyanGFX 驱动 + LVGL 8 移植）
 * @stage   阶段二（LCD 验证）/ 阶段三（LVGL 界面）/ 阶段六（完善状态显示）
 *
 * 依赖：LovyanGFX（LCD SPI 驱动）、LVGL 8.3.x
 * 引脚：见 config.h 的 LCD_SCK_PIN / LCD_MOSI_PIN / LCD_MISO_PIN /
 *       LCD_CS_PIN / LCD_DC_PIN / LCD_RST_PIN / LCD_BL_PIN
 *       ★ 烧录前务必对照正点原子 DNEP32S3 官方 LCD 例程核对这些引脚。
 *
 * LVGL 配置要求（在 lv_conf.h 中开启）：
 *   #define LV_TICK_CUSTOM 1
 *   #define LV_TICK_CUSTOM_INCLUDE <Arduino.h>
 *   #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
 *   显存建议：LV_MEM_SIZE 至少 (64 * 1024)，LV_COLOR_DEPTH 16。
 */
#include "display_ui.h"
#include <lvgl.h>
#include <LovyanGFX.hpp>

// ---------- LovyanGFX 板级配置（引脚来自 config.h） ----------
class LGFX : public lgfx::LGFX_Device {
    lgfx::Bus_SPI   _bus;
    lgfx::Panel_ILI9341 _panel;
public:
    LGFX(void) {
        auto bus = _bus.config();
#if defined(SPI3_HOST)
        bus.spi_host = SPI3_HOST;     // ESP32-S3 的 VSPI；旧版 Arduino-ESP32 请改为 VSPI_HOST
#else
        bus.spi_host = VSPI_HOST;
#endif
        bus.spi_mode = 0;
        bus.freq_write = 40000000;
        bus.freq_read  = 16000000;
        bus.pin_sclk = LCD_SCK_PIN;
        bus.pin_mosi = LCD_MOSI_PIN;
        bus.pin_miso = LCD_MISO_PIN;
        bus.pin_dc   = LCD_DC_PIN;
        _bus.config(bus);
        _bus.init();

        auto pnl = _panel.config();
        pnl.bus = &_bus;              // 面板挂载到 SPI 总线
        pnl.pin_cs   = LCD_CS_PIN;
        pnl.pin_rst  = LCD_RST_PIN;
        pnl.pin_bl   = LCD_BL_PIN;    // -1 表示无背光控制引脚
        pnl.bl_on_level = 1;
        pnl.offset_rotation = 0;
        pnl.readable = false;
        pnl.invert = false;
        pnl.rgb_order = false;
        pnl.dlen_16bit = false;
        pnl.width  = 320;
        pnl.height = 240;
        _panel.config(pnl);
        _panel.init();
        setPanel(&_panel);            // 设备挂载面板
    }
};
static LGFX gfx;

// ---------- LVGL 显示端口 ----------
#define DISP_BUF_LINES 40
static lv_color_t disp_buf1[320 * DISP_BUF_LINES];
static lv_color_t disp_buf2[320 * DISP_BUF_LINES];
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    gfx.pushImage(area->x1, area->y1, w, h, (uint16_t *)color_p);
    lv_disp_flush_ready(drv);
}

static lv_obj_t *label_wifi;
static lv_obj_t *label_bat;
static lv_obj_t *label_mode;
static lv_obj_t *label_result;
static lv_obj_t *label_sd;

void display_init() {
    gfx.init();
    if (LCD_BL_PIN >= 0) { pinMode(LCD_BL_PIN, OUTPUT); digitalWrite(LCD_BL_PIN, HIGH); }

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, disp_buf1, disp_buf2, 320 * DISP_BUF_LINES);
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 320;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    lv_obj_t *scr = lv_scr_act();
    label_wifi   = lv_label_create(scr);  lv_label_set_text(label_wifi,   "WiFi: --");
    label_bat    = lv_label_create(scr);  lv_label_set_text(label_bat,    "Bat : --%");
    label_mode   = lv_label_create(scr);  lv_label_set_text(label_mode,   "Mode: BOOT");
    label_result = lv_label_create(scr);  lv_label_set_text(label_result, "Res : --");
    label_sd     = lv_label_create(scr);  lv_label_set_text(label_sd,     "SD  : --KB");

    lv_obj_align(label_wifi,   LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_align(label_bat,    LV_ALIGN_TOP_LEFT, 0, 24);
    lv_obj_align(label_mode,   LV_ALIGN_TOP_LEFT, 0, 48);
    lv_obj_align(label_result, LV_ALIGN_TOP_LEFT, 0, 72);
    lv_obj_align(label_sd,     LV_ALIGN_TOP_LEFT, 0, 96);
    lv_timer_handler();
}

void display_update(int rssi, int battery_pct, const char *mode,
                    const char *last_result, int sd_free_kb) {
    char buf[64];
    snprintf(buf, sizeof(buf), "WiFi: %ddBm", rssi);  lv_label_set_text(label_wifi, buf);
    snprintf(buf, sizeof(buf), "Bat : %d%%", battery_pct); lv_label_set_text(label_bat, buf);
    lv_label_set_text(label_mode, mode);
    lv_label_set_text(label_result, last_result);
    snprintf(buf, sizeof(buf), "SD  : %dKB", sd_free_kb); lv_label_set_text(label_sd, buf);
    lv_timer_handler();  // 驱动 LVGL 刷新
}
