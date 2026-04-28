// C 標準函式庫
#include <stdio.h>
#include <string.h>

// ESP-IDF 系統組件
#include <esp_log.h>

// 第三方組件
#include <lvgl.h>
#include <esp_lvgl_port.h>

// 本專案模組
#include "fonts/fonts.h"
#include "module_alarm.h"
#include "module_ui_alarm_edit.h"

// 日誌標籤
static const char *TAG = "M_UI_AlarmEdit";

// ======================================================================
// 私有巨集
// ======================================================================

#define PANEL_W         400
#define PANEL_H         260
#define HEADER_H        36
#define ROLLER_W        80
#define DAY_BTN_W       44
#define DAY_BTN_H       38
#define BTN_W           90
#define BTN_H           36

// 星期幾標籤，index 1-7 對應 weekday bit 1-7（1=週日…7=週六）
static const char *DAY_LABELS[] = { "", "日", "一", "二", "三", "四", "五", "六" };

// ======================================================================
// 私有變數
// ======================================================================

static lv_obj_t            *s_backdrop    = NULL;
static lv_obj_t            *s_roller_h    = NULL;
static lv_obj_t            *s_roller_m    = NULL;
static lv_obj_t            *s_day_btns[8];        // index 1-7 有效
static alarm_edit_done_cb_t s_done_cb     = NULL;
static int                  s_editing_slot = -1;

// ======================================================================
// 私有函式前向宣告
// ======================================================================

static void build_range_str(char *buf, size_t bufsize, int min, int max);
static uint8_t read_repeat_days(void);
static void on_save_click(lv_event_t *e);
static void on_cancel_click(lv_event_t *e);
static void on_day_btn_click(lv_event_t *e);

// ======================================================================
// 私有函式實作
// ======================================================================

static void build_range_str(char *buf, size_t bufsize, int min, int max)
{
    size_t pos = 0;
    for (int i = min; i <= max; i++) {
        int written;
        if (i == min) {
            written = snprintf(buf + pos, bufsize - pos, "%02d", i);
        } else {
            written = snprintf(buf + pos, bufsize - pos, "\n%02d", i);
        }
        if (written < 0 || (size_t)written >= bufsize - pos) break;
        pos += (size_t)written;
    }
}

static uint8_t read_repeat_days(void)
{
    uint8_t mask = 0;
    for (int d = 1; d <= 7; d++) {
        if (lv_obj_has_state(s_day_btns[d], LV_STATE_CHECKED)) {
            mask |= ALARM_WEEKDAY_BIT(d);
        }
    }
    return mask;
}

static void on_save_click(lv_event_t *e)
{
    (void)e;
    alarm_t result = {
        .hour        = (uint8_t)lv_roller_get_selected(s_roller_h),
        .minute      = (uint8_t)lv_roller_get_selected(s_roller_m),
        .repeat_days = read_repeat_days(),
        .enabled     = 1,
    };
    alarm_edit_done_cb_t cb = s_done_cb;
    int slot = s_editing_slot;
    module_ui_alarm_edit_close();
    if (cb) cb(true, &result, slot);
}

static void on_cancel_click(lv_event_t *e)
{
    (void)e;
    alarm_edit_done_cb_t cb = s_done_cb;
    int slot = s_editing_slot;
    module_ui_alarm_edit_close();
    if (cb) cb(false, NULL, slot);
}

static void on_day_btn_click(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    if (lv_obj_has_state(btn, LV_STATE_CHECKED)) {
        lv_obj_remove_state(btn, LV_STATE_CHECKED);
    } else {
        lv_obj_add_state(btn, LV_STATE_CHECKED);
    }
}

// ======================================================================
// 公開 API
// ======================================================================

void module_ui_alarm_edit_open(int slot, const alarm_t *initial, alarm_edit_done_cb_t cb)
{
    if (s_backdrop != NULL) return;

    s_done_cb      = cb;
    s_editing_slot = slot;
    memset(s_day_btns, 0, sizeof(s_day_btns));

    // 預設值
    uint8_t init_hour = 8, init_minute = 0, init_repeat = 0;
    if (initial) {
        init_hour   = initial->hour;
        init_minute = initial->minute;
        init_repeat = initial->repeat_days;
    }

    lvgl_port_lock(0);

    // 半透明背板
    s_backdrop = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(s_backdrop);
    lv_obj_set_size(s_backdrop, LV_PCT(100), LV_PCT(100));
    lv_obj_center(s_backdrop);
    lv_obj_set_style_bg_color(s_backdrop, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_backdrop, LV_OPA_70, 0);
    lv_obj_clear_flag(s_backdrop, LV_OBJ_FLAG_SCROLLABLE);

    // 中央面板
    lv_obj_t *panel = lv_obj_create(s_backdrop);
    lv_obj_set_size(panel, PANEL_W, PANEL_H);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_make(25, 25, 25), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, lv_color_make(55, 55, 55), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 12, 0);
    lv_obj_set_style_pad_all(panel, 12, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    // 標題
    lv_obj_t *lbl_title = lv_label_create(panel);
    lv_label_set_text(lbl_title, slot == -1 ? "新增鬧鐘" : "編輯鬧鐘");
    lv_obj_set_style_text_font(lbl_title, &font_huninn_18, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 4);

    // ------------------------------------------------------------------
    // 時 roller
    // ------------------------------------------------------------------
    char hour_opts[24 * 4];
    build_range_str(hour_opts, sizeof(hour_opts), 0, 23);

    s_roller_h = lv_roller_create(panel);
    lv_roller_set_options(s_roller_h, hour_opts, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(s_roller_h, 3);
    lv_obj_set_width(s_roller_h, ROLLER_W);
    lv_obj_set_style_text_font(s_roller_h, &font_huninn_24, 0);
    lv_obj_set_style_bg_color(s_roller_h, lv_color_make(35, 35, 35), 0);
    lv_obj_set_style_text_color(s_roller_h, lv_color_make(180, 180, 180), 0);
    lv_obj_set_style_text_color(s_roller_h, lv_color_white(), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(s_roller_h, lv_color_make(55, 55, 55), LV_PART_SELECTED);
    lv_obj_align(s_roller_h, LV_ALIGN_TOP_LEFT, 90, HEADER_H + 8);
    lv_roller_set_selected(s_roller_h, init_hour, LV_ANIM_OFF);

    // 分隔 ":"
    lv_obj_t *lbl_colon = lv_label_create(panel);
    lv_label_set_text(lbl_colon, ":");
    lv_obj_set_style_text_font(lbl_colon, &font_huninn_32, 0);
    lv_obj_set_style_text_color(lbl_colon, lv_color_white(), 0);
    lv_obj_align(lbl_colon, LV_ALIGN_TOP_MID, 0, HEADER_H + 22);

    // ------------------------------------------------------------------
    // 分 roller
    // ------------------------------------------------------------------
    char min_opts[60 * 4];
    build_range_str(min_opts, sizeof(min_opts), 0, 59);

    s_roller_m = lv_roller_create(panel);
    lv_roller_set_options(s_roller_m, min_opts, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(s_roller_m, 3);
    lv_obj_set_width(s_roller_m, ROLLER_W);
    lv_obj_set_style_text_font(s_roller_m, &font_huninn_24, 0);
    lv_obj_set_style_bg_color(s_roller_m, lv_color_make(35, 35, 35), 0);
    lv_obj_set_style_text_color(s_roller_m, lv_color_make(180, 180, 180), 0);
    lv_obj_set_style_text_color(s_roller_m, lv_color_white(), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(s_roller_m, lv_color_make(55, 55, 55), LV_PART_SELECTED);
    lv_obj_align(s_roller_m, LV_ALIGN_TOP_RIGHT, -90, HEADER_H + 8);
    lv_roller_set_selected(s_roller_m, init_minute, LV_ANIM_OFF);

    // ------------------------------------------------------------------
    // 星期幾 toggle 按鈕（日一二三四五六，對應 weekday bit 1-7）
    // ------------------------------------------------------------------
    // 7 個按鈕排成一行，置中對齊
    int total_day_w = 7 * DAY_BTN_W + 6 * 4;   // 7 個按鈕 + 6 個間距
    int day_start_x = (PANEL_W - 24 - total_day_w) / 2;   // 扣掉兩側 padding 後置中

    // 計算 roller 高度以對齊 day buttons 的 Y 位置
    // lv_roller 的高度由 visible_row_count 和 font 決定，抓 roller 高度後定位
    lv_obj_t *day_row = lv_obj_create(panel);
    lv_obj_remove_style_all(day_row);
    lv_obj_set_size(day_row, total_day_w, DAY_BTN_H);
    lv_obj_align(day_row, LV_ALIGN_BOTTOM_MID, 0, -(BTN_H + 12));
    lv_obj_set_style_pad_all(day_row, 0, 0);
    lv_obj_set_style_pad_column(day_row, 4, 0);
    lv_obj_set_layout(day_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(day_row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(day_row, LV_OBJ_FLAG_SCROLLABLE);
    (void)day_start_x;

    for (int d = 1; d <= 7; d++) {
        lv_obj_t *btn = lv_button_create(day_row);
        lv_obj_set_size(btn, DAY_BTN_W, DAY_BTN_H);
        lv_obj_set_style_bg_color(btn, lv_color_make(45, 45, 45), 0);
        lv_obj_set_style_bg_color(btn, lv_color_make(60, 140, 90), LV_STATE_CHECKED);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_add_event_cb(btn, on_day_btn_click, LV_EVENT_CLICKED, NULL);

        if (init_repeat & ALARM_WEEKDAY_BIT(d)) {
            lv_obj_add_state(btn, LV_STATE_CHECKED);
        }

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, DAY_LABELS[d]);
        lv_obj_set_style_text_font(lbl, &font_huninn_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        lv_obj_center(lbl);

        s_day_btns[d] = btn;
    }

    // ------------------------------------------------------------------
    // 取消 / 儲存 按鈕
    // ------------------------------------------------------------------
    lv_obj_t *btn_cancel = lv_button_create(panel);
    lv_obj_set_size(btn_cancel, BTN_W, BTN_H);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_RIGHT, -(BTN_W + 8), 0);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_make(50, 50, 50), 0);
    lv_obj_set_style_radius(btn_cancel, 8, 0);
    lv_obj_add_event_cb(btn_cancel, on_cancel_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "取消");
    lv_obj_set_style_text_font(lbl_cancel, &font_huninn_18, 0);
    lv_obj_set_style_text_color(lbl_cancel, lv_color_white(), 0);
    lv_obj_center(lbl_cancel);

    lv_obj_t *btn_save = lv_button_create(panel);
    lv_obj_set_size(btn_save, BTN_W, BTN_H);
    lv_obj_align(btn_save, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(btn_save, lv_color_make(60, 140, 90), 0);
    lv_obj_set_style_radius(btn_save, 8, 0);
    lv_obj_add_event_cb(btn_save, on_save_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "儲存");
    lv_obj_set_style_text_font(lbl_save, &font_huninn_18, 0);
    lv_obj_set_style_text_color(lbl_save, lv_color_white(), 0);
    lv_obj_center(lbl_save);

    lvgl_port_unlock();
    ESP_LOGI(TAG, "編輯對話框開啟 (slot=%d)", slot);
}

void module_ui_alarm_edit_close(void)
{
    if (s_backdrop == NULL) return;

    lvgl_port_lock(0);
    lv_obj_delete(s_backdrop);
    lvgl_port_unlock();

    s_backdrop     = NULL;
    s_roller_h     = NULL;
    s_roller_m     = NULL;
    s_done_cb      = NULL;
    s_editing_slot = -1;
    memset(s_day_btns, 0, sizeof(s_day_btns));

    ESP_LOGI(TAG, "編輯對話框關閉");
}
