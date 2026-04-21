// ESP-IDF 系統組件
#include <esp_log.h>

// 第三方組件
#include <lvgl.h>
#include <esp_lvgl_port.h>

// 本專案模組
#include "fonts/fonts.h"
#include "module_ui_alarm.h"

static const char *TAG = "M_UI_Alarm";

// ======================================================================
// 版面尺寸
// ======================================================================

#define PANEL_W         400
#define PANEL_H         260
#define HEADER_H        36
#define ITEM_H          54
#define ITEM_GAP        6
#define ADD_BTN_W       72
#define DONE_BTN_W      100
#define DONE_BTN_H      36

// ======================================================================
// 私有變數
// ======================================================================

static lv_obj_t *s_backdrop = NULL;

// ======================================================================
// Mock 資料（實際鬧鐘邏輯尚未實作）
// ======================================================================

typedef struct {
    const char *time;
    const char *label;
    bool        enabled;
} mock_alarm_t;

static const mock_alarm_t MOCK_ALARMS[] = {
    { "07:30", "工作日",     true  },
    { "09:00", "週末",       true  },
    { "13:15", "午休提醒",   false },
};
static const int MOCK_ALARM_COUNT = sizeof(MOCK_ALARMS) / sizeof(MOCK_ALARMS[0]);

// ======================================================================
// 事件 callback（mock — 只記錄日誌，不實際變更狀態）
// ======================================================================

static void backdrop_click_cb(lv_event_t *e)
{
    if (lv_event_get_target(e) != s_backdrop) return;
    module_ui_alarm_close();
}

static void done_click_cb(lv_event_t *e)
{
    (void)e;
    module_ui_alarm_close();
}

static void add_click_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "[mock] 點擊 新增鬧鐘");
}

static void item_click_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    ESP_LOGI(TAG, "[mock] 點擊鬧鐘項目 %d (%s)", idx, MOCK_ALARMS[idx].time);
}

static void delete_click_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    ESP_LOGI(TAG, "[mock] 點擊刪除鬧鐘 %d (%s)", idx, MOCK_ALARMS[idx].time);
    lv_event_stop_bubbling(e);
}

static void switch_change_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t *sw = lv_event_get_target(e);
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    ESP_LOGI(TAG, "[mock] 鬧鐘 %d (%s) 切換為 %s",
             idx, MOCK_ALARMS[idx].time, on ? "開" : "關");
}

// ======================================================================
// 私有建構函式
// ======================================================================

static void build_alarm_item(lv_obj_t *parent, int idx, const mock_alarm_t *a)
{
    lv_obj_t *item = lv_obj_create(parent);
    lv_obj_set_size(item, LV_PCT(100), ITEM_H);
    lv_obj_set_style_bg_color(item, lv_color_make(35, 35, 35), 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(item, lv_color_make(55, 55, 55), 0);
    lv_obj_set_style_border_width(item, 1, 0);
    lv_obj_set_style_radius(item, 8, 0);
    lv_obj_set_style_pad_hor(item, 12, 0);
    lv_obj_set_style_pad_ver(item, 6, 0);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(item, item_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);

    lv_obj_t *lbl_time = lv_label_create(item);
    lv_label_set_text(lbl_time, a->time);
    lv_obj_set_style_text_font(lbl_time, &font_huninn_24, 0);
    lv_obj_set_style_text_color(lbl_time,
        a->enabled ? lv_color_white() : lv_color_make(140, 140, 140), 0);
    lv_obj_align(lbl_time, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *lbl_label = lv_label_create(item);
    lv_label_set_text(lbl_label, a->label);
    lv_obj_set_style_text_font(lbl_label, &font_huninn_14, 0);
    lv_obj_set_style_text_color(lbl_label, lv_color_make(170, 170, 170), 0);
    lv_obj_align(lbl_label, LV_ALIGN_LEFT_MID, 80, 0);

    lv_obj_t *sw = lv_switch_create(item);
    lv_obj_set_size(sw, 40, 22);
    if (a->enabled) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -52, 0);
    lv_obj_add_event_cb(sw, switch_change_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)idx);

    lv_obj_t *btn_del = lv_button_create(item);
    lv_obj_set_size(btn_del, 44, 28);
    lv_obj_set_style_bg_color(btn_del, lv_color_make(60, 60, 60), 0);
    lv_obj_set_style_radius(btn_del, 6, 0);
    lv_obj_align(btn_del, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(btn_del, delete_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);

    lv_obj_t *lbl_del = lv_label_create(btn_del);
    lv_label_set_text(lbl_del, "移除");
    lv_obj_set_style_text_font(lbl_del, &font_huninn_14, 0);
    lv_obj_set_style_text_color(lbl_del, lv_color_white(), 0);
    lv_obj_center(lbl_del);
}

// ======================================================================
// 公開 API
// ======================================================================

void module_ui_alarm_open(void)
{
    if (s_backdrop != NULL) return;

    lvgl_port_lock(0);

    // ------------------------------------------------------------------
    // 半透明暗色背板（全螢幕可點擊 → 關閉）
    // ------------------------------------------------------------------
    s_backdrop = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(s_backdrop);
    lv_obj_set_size(s_backdrop, LV_PCT(100), LV_PCT(100));
    lv_obj_center(s_backdrop);
    lv_obj_set_style_bg_color(s_backdrop, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_backdrop, LV_OPA_70, 0);
    lv_obj_clear_flag(s_backdrop, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_backdrop, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_backdrop, backdrop_click_cb, LV_EVENT_CLICKED, NULL);

    // ------------------------------------------------------------------
    // 中央面板
    // ------------------------------------------------------------------
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

    // ------------------------------------------------------------------
    // 標題列：鬧鐘 + 新增鈕
    // ------------------------------------------------------------------
    lv_obj_t *header = lv_obj_create(panel);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, LV_PCT(100), HEADER_H);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *lbl_title = lv_label_create(header);
    lv_label_set_text(lbl_title, "鬧鐘");
    lv_obj_set_style_text_font(lbl_title, &font_huninn_24, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
    lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *btn_add = lv_button_create(header);
    lv_obj_set_size(btn_add, ADD_BTN_W, 32);
    lv_obj_align(btn_add, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_add, lv_color_make(60, 140, 90), 0);
    lv_obj_set_style_radius(btn_add, 8, 0);
    lv_obj_add_event_cb(btn_add, add_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_add = lv_label_create(btn_add);
    lv_label_set_text(lbl_add, "+ 新增");
    lv_obj_set_style_text_font(lbl_add, &font_huninn_14, 0);
    lv_obj_set_style_text_color(lbl_add, lv_color_white(), 0);
    lv_obj_center(lbl_add);

    // ------------------------------------------------------------------
    // 鬧鐘列表（可垂直捲動）
    // ------------------------------------------------------------------
    lv_obj_t *list = lv_obj_create(panel);
    lv_obj_set_size(list, LV_PCT(100), PANEL_H - 12 * 2 - HEADER_H - DONE_BTN_H - 8);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, HEADER_H + 4);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_row(list, ITEM_GAP, 0);
    lv_obj_set_layout(list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);

    for (int i = 0; i < MOCK_ALARM_COUNT; i++) {
        build_alarm_item(list, i, &MOCK_ALARMS[i]);
    }

    // ------------------------------------------------------------------
    // 底部完成按鈕
    // ------------------------------------------------------------------
    lv_obj_t *btn_done = lv_button_create(panel);
    lv_obj_set_size(btn_done, DONE_BTN_W, DONE_BTN_H);
    lv_obj_align(btn_done, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(btn_done, lv_color_make(45, 45, 45), 0);
    lv_obj_set_style_radius(btn_done, 8, 0);
    lv_obj_add_event_cb(btn_done, done_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_done = lv_label_create(btn_done);
    lv_label_set_text(lbl_done, "完成");
    lv_obj_set_style_text_font(lbl_done, &font_huninn_18, 0);
    lv_obj_set_style_text_color(lbl_done, lv_color_white(), 0);
    lv_obj_center(lbl_done);

    lvgl_port_unlock();
    ESP_LOGI(TAG, "鬧鐘 UI 開啟");
}

void module_ui_alarm_close(void)
{
    if (s_backdrop == NULL) return;

    lvgl_port_lock(0);
    lv_obj_delete(s_backdrop);
    lvgl_port_unlock();

    s_backdrop = NULL;
    ESP_LOGI(TAG, "鬧鐘 UI 關閉");
}
