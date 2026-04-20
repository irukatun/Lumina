// ESP-IDF 系統組件
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

// 第三方組件
#include <lvgl.h>
#include <esp_lvgl_port.h>

// 本專案模組
#include "board.h"
#include "fonts.h"
#include "module_display.h"
#include "module_ui_boot.h"

// 日誌標籤
static const char *TAG = "M_UI_Boot";

// ======================================================================
// 私有變數
// ======================================================================
static lv_obj_t *s_bar          = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_spinner      = NULL;

// ======================================================================
// 私有函式前向宣告
// ======================================================================
static void backlight_fadein(void);
static void ui_fadein(void);
static void arc_fill_anim_cb(void *obj, int32_t v);

// ======================================================================
// 私有函式實作
// ======================================================================
static void arc_fill_anim_cb(void *obj, int32_t v)
{
    lv_arc_set_end_angle((lv_obj_t *)obj, (uint16_t)(v % 360));
}

static void backlight_fadein(void)
{
    for (int i = 0; i <= 100; i++) {
        module_display_backlight_set(i);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void ui_fadein(void)
{
    for (int opa = 0; opa <= 255; opa += 5) {
        lvgl_port_lock(0);
        lv_obj_set_style_opa(s_bar,          opa, 0);
        lv_obj_set_style_opa(s_status_label, opa, 0);
        lv_obj_set_style_opa(s_spinner,      opa, 0);
        lvgl_port_unlock();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    lvgl_port_lock(0);
    lv_obj_set_style_opa(s_bar,          LV_OPA_COVER, 0);
    lv_obj_set_style_opa(s_status_label, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(s_spinner,      LV_OPA_COVER, 0);
    lvgl_port_unlock();
}

// ======================================================================
// 公開 API
// ======================================================================
// 建立開機畫面並播放進場動畫（阻塞）
void module_ui_boot_show(void)
{
    ESP_LOGI(TAG, "建立開機畫面");

    lvgl_port_lock(0);

    lv_obj_t *scr = lv_screen_active();

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // 主標題
    lv_obj_t *lbl_title = lv_label_create(scr);
    lv_label_set_text(lbl_title, "LUMINA");
    lv_obj_set_style_text_font(lbl_title, &font_huninn_32, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
    lv_obj_set_style_text_letter_space(lbl_title, 8, 0);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, -75);

    // 副標題
    lv_obj_t *lbl_subtitle = lv_label_create(scr);
    lv_label_set_text(lbl_subtitle, "小鹿米智慧桌上助理");
    lv_obj_set_style_text_font(lbl_subtitle, &font_huninn_18, 0);
    lv_obj_set_style_text_color(lbl_subtitle, lv_color_make(200, 200, 200), 0);
    lv_obj_align(lbl_subtitle, LV_ALIGN_CENTER, 0, -35);

    // 版本號
    lv_obj_t *lbl_ver = lv_label_create(scr);
    lv_label_set_text(lbl_ver, "v" FIRMWARE_VERSION);
    lv_obj_set_style_text_font(lbl_ver, &font_huninn_14, 0);
    lv_obj_set_style_text_color(lbl_ver, lv_color_make(120, 120, 120), 0);
    lv_obj_align(lbl_ver, LV_ALIGN_CENTER, 0, -5);

    // 初始化狀態文字（進度條上方）
    s_status_label = lv_label_create(scr);
    lv_label_set_text(s_status_label, "正在初始化...");
    lv_obj_set_style_text_font(s_status_label, &font_huninn_14, 0);
    lv_obj_set_style_text_color(s_status_label, lv_color_make(160, 160, 160), 0);
    lv_obj_align(s_status_label, LV_ALIGN_CENTER, 0, 72);
    lv_obj_set_style_opa(s_status_label, LV_OPA_TRANSP, 0);

    // 進度條
    s_bar = lv_bar_create(scr);
    lv_obj_set_size(s_bar, 320, 6);
    lv_obj_align(s_bar, LV_ALIGN_CENTER, 0, 95);
    lv_obj_set_style_bg_color(s_bar, lv_color_make(60, 60, 60), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_anim_duration(s_bar, 1000, 0);
    lv_obj_set_style_opa(s_bar, LV_OPA_TRANSP, 0);
    lv_bar_set_range(s_bar, 0, 100);
    lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);

    // 右下角旋轉 Spinner
    s_spinner = lv_spinner_create(scr);
    lv_obj_set_size(s_spinner, 36, 36);
    lv_obj_align(s_spinner, LV_ALIGN_BOTTOM_RIGHT, -18, -18);
    lv_spinner_set_anim_params(s_spinner, 1000, 300);
    lv_obj_set_style_arc_color(s_spinner, lv_color_make(60, 60, 60), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_spinner, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_spinner, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_spinner, 3, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_spinner, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(s_spinner, LV_OPA_TRANSP, 0);

    lvgl_port_unlock();

    backlight_fadein();
    ui_fadein();
}

// 更新開機畫面進度條
void module_ui_boot_set_progress(int value)
{
    if (s_bar == NULL) return;

    lvgl_port_lock(0);
    lv_bar_set_value(s_bar, value, LV_ANIM_ON);
    lvgl_port_unlock();
}

// 更新開機畫面狀態文字
void module_ui_boot_set_status(const char *text)
{
    if (s_status_label == NULL) return;

    lvgl_port_lock(0);
    lv_label_set_text(s_status_label, text);
    lvgl_port_unlock();
}

// 完成開機：停止旋轉、弧線填滿並顯示打勾圖標（非阻塞）
void module_ui_boot_set_complete(void)
{
    if (s_spinner == NULL) return;

    lvgl_port_lock(0);

    // 停止旋轉動畫，重置至 12 點鐘位置
    lv_anim_delete(s_spinner, NULL);
    lv_arc_set_angles(s_spinner, 270, 270);

    // 以 ease-out 動畫將弧線填滿一整圈
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_spinner);
    lv_anim_set_exec_cb(&a, arc_fill_anim_cb);
    lv_anim_set_values(&a, 270, 270 + 359);
    lv_anim_set_duration(&a, 500);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    // 中間顯示打勾圖標
    lv_obj_t *lbl_ok = lv_label_create(lv_obj_get_parent(s_spinner));
    lv_label_set_text(lbl_ok, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(lbl_ok, lv_color_white(), 0);
    lv_obj_align_to(lbl_ok, s_spinner, LV_ALIGN_CENTER, 0, 0);

    lvgl_port_unlock();
}
