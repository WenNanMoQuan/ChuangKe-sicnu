/**
 * @file    display_ui.cpp
 * @brief   LVGL 状态界面实现（骨架，按正点原子官方 LCD 驱动补全底层）
 * @stage   阶段二 / 阶段三 / 阶段六
 */
#include "display_ui.h"
#include <lvgl.h>

// 说明：以下 LVGL 对象创建依赖正点原子 LCD 官方驱动（SPI）提供的
// lvgl 移植层（lv_port_disp_init）。若官方例程有 display 接口，直接替换即可。

static lv_obj_t *label_wifi;
static lv_obj_t *label_bat;
static lv_obj_t *label_mode;
static lv_obj_t *label_result;
static lv_obj_t *label_sd;

void display_init() {
    lv_init();
    // lv_port_disp_init();  // 由正点原子官方 LCD 例程提供的移植函数实现
    lv_obj_t *scr = lv_scr_act();

    label_wifi   = lv_label_create(scr);  lv_label_set_text(label_wifi,   "WiFi: --");
    label_bat    = lv_label_create(scr);  lv_label_set_text(label_bat,    "Bat : --%");
    label_mode   = lv_label_create(scr);  lv_label_set_text(label_mode,   "Mode: BOOT");
    label_result = lv_label_create(scr);  lv_label_set_text(label_result, "Res : --");
    label_sd     = lv_label_create(scr);  lv_label_set_text(label_sd,     "SD  : --KB");

    lv_obj_align(label_wifi,   LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_align(label_bat,    LV_ALIGN_TOP_LEFT, 0, 20);
    lv_obj_align(label_mode,   LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_align(label_result, LV_ALIGN_TOP_LEFT, 0, 60);
    lv_obj_align(label_sd,     LV_ALIGN_TOP_LEFT, 0, 80);
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
