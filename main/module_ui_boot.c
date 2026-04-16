// ESP-IDF 系統組件
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

// 第三方組件
#include <lvgl.h>
#include <esp_lvgl_port.h>

// 本專案模組
#include "board.h"
#include "module_ui_boot.h"

// 日誌標籤
static const char *TAG = "M_UI_Boot";

// ======================================================================
// 私有變數
// ======================================================================
static lv_obj_t *s_bar = NULL;

// ======================================================================
// 公開 API
// ======================================================================
// 建立並顯示開機畫面（非阻塞）
void module_ui_boot_show(void)
{
    ESP_LOGI(TAG, "建立開機畫面");

    lvgl_port_lock(0);

    lv_obj_t *scr = lv_screen_active();

    // 暖奶白底色（晨光感）
    lv_obj_set_style_bg_color(scr, lv_color_make(0xF0, 0xE6, 0xDC), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // 標題 "LUMINA"（深焦茶色，寬字距，品牌感）
    lv_obj_t *lbl_title = lv_label_create(scr);
    lv_label_set_text(lbl_title, "LUMINA");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_make(0x24, 0x18, 0x10), 0);
    lv_obj_set_style_text_letter_space(lbl_title, 8, 0);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, -60);

    // 版本號（暖灰褐，低調副文字）
    lv_obj_t *lbl_ver = lv_label_create(scr);
    lv_label_set_text(lbl_ver, "v" FIRMWARE_VERSION);
    lv_obj_set_style_text_font(lbl_ver, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_ver, lv_color_make(0x9A, 0x84, 0x78), 0);
    lv_obj_align(lbl_ver, LV_ALIGN_CENTER, 0, -20);

    // 進度條（細條，燒赭色指示，融入底色的 track）
    s_bar = lv_bar_create(scr);
    lv_obj_set_size(s_bar, 320, 6);
    lv_obj_align(s_bar, LV_ALIGN_CENTER, 0, 100);
    lv_obj_set_style_bg_color(s_bar, lv_color_make(0xDD, 0xD0, 0xC8), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar, lv_color_make(0xB8, 0x60, 0x40), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_bar_set_range(s_bar, 0, 100);
    lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);

    lvgl_port_unlock();
}

// 更新開機畫面進度條
void module_ui_boot_set_progress(int value)
{
    if (s_bar == NULL) return;

    lvgl_port_lock(0);
    lv_bar_set_value(s_bar, value, LV_ANIM_ON);
    lvgl_port_unlock();
}
