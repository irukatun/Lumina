// C 標準函式庫
#include <stdio.h>

// ESP-IDF 系統組件
#include <esp_log.h>

// 第三方組件
#include <lvgl.h>
#include <esp_lvgl_port.h>

// 本專案模組
#include "board.h"
#include "fonts/fonts.h"
#include "module_time.h"
#include "module_imu.h"
#include "module_pir.h"
#include "module_ui_alarm.h"
#include "module_ui_main.h"

// 日誌標籤
static const char *TAG = "M_UI_Main";

// ======================================================================
// 私有巨集
// ======================================================================

// 版面尺寸
#define STATUS_BAR_H    24
#define LEFT_PANEL_W    270
#define LEFT_HALF_H     (MAIN_AREA_H / 2)
#define DIVIDER_W       1
#define RIGHT_PANEL_W   (CONFIG_ILI9488_WIDTH - LEFT_PANEL_W - DIVIDER_W)
#define MAIN_AREA_H     (CONFIG_ILI9488_HEIGHT - STATUS_BAR_H)

// 右側資訊卡片
#define CARD_INNER_W    (RIGHT_PANEL_W - 16)
#define CARD_H          65
#define CARD_GAP        8

// 左下 IMU 單一卡片
#define IMU_CARD_W      240
#define IMU_CARD_H      96

// 敲擊顯示自動消退秒數
#define TAP_CLEAR_TICKS 3

// ======================================================================
// 私有變數
// ======================================================================

static const char *WEEKDAY_STR[] = {
    "", "週日", "週一", "週二", "週三", "週四", "週五", "週六"
};

static lv_obj_t  *s_scr             = NULL;

// 狀態列
static lv_obj_t  *s_lbl_date_status = NULL;
static lv_obj_t  *s_lbl_pir_status  = NULL;

// 左上：時鐘
static lv_obj_t  *s_lbl_time        = NULL;  // HH:MM
static lv_obj_t  *s_lbl_sec         = NULL;  // :SS
static lv_obj_t  *s_lbl_alarm       = NULL;  // 下一個鬧鐘提示（保留供未來更新）

// 左下：IMU 單一卡片
static lv_obj_t  *s_lbl_imu_val     = NULL;

// 右側資訊卡片
static lv_obj_t  *s_lbl_temp_val    = NULL;

// LVGL timer
static lv_timer_t *s_clock_timer    = NULL;

// 敲擊自動消退計數（兩個任務都在 lvgl_port_lock 內存取，無競態）
static int s_tap_clear_ticks = 0;

// ======================================================================
// 私有函式前向宣告
// ======================================================================

static lv_obj_t *create_right_card(lv_obj_t *parent, const char *title, int y);
static void      refresh_clock(void);
static void      clock_timer_cb(lv_timer_t *timer);
static void      on_imu_event(imu_event_t event);
static void      on_pir_event(pir_event_t event);
static void      on_time_clicked(lv_event_t *e);

// ======================================================================
// 私有函式實作
// ======================================================================

static lv_obj_t *create_right_card(lv_obj_t *parent, const char *title, int y)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, CARD_INNER_W, CARD_H);
    lv_obj_set_pos(card, 0, y);
    lv_obj_set_style_bg_color(card, lv_color_make(25, 25, 25), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_make(55, 55, 55), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, &font_huninn_14, 0);
    lv_obj_set_style_text_color(lbl, lv_color_make(190, 190, 190), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    return card;
}

// 更新時鐘與狀態列（在 LVGL lock 內呼叫）
static void refresh_clock(void)
{
    module_time_t t;
    if (module_time_get(&t) != ESP_OK) return;

    char buf[48];

    snprintf(buf, sizeof(buf), "%02d:%02d", t.hour, t.minute);
    lv_label_set_text(s_lbl_time, buf);

    snprintf(buf, sizeof(buf), ":%02d", t.second);
    lv_label_set_text(s_lbl_sec, buf);

    const char *wd = (t.weekday >= 1 && t.weekday <= 7) ? WEEKDAY_STR[t.weekday] : "---";
    snprintf(buf, sizeof(buf), "%d年%02d月%02d日 %s", t.year, t.month, t.day, wd);
    lv_label_set_text(s_lbl_date_status, buf);
}

// 每秒觸發，由 LVGL 任務呼叫（已在 lock 內，不可重入）
static void clock_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    refresh_clock();

    // 敲擊顯示自動消退
    if (s_tap_clear_ticks > 0 && --s_tap_clear_ticks == 0) {
        lv_label_set_text(s_lbl_imu_val, "—");
        lv_obj_set_style_text_color(s_lbl_imu_val, lv_color_make(180, 180, 180), 0);
    }

    // DS3231 溫度暫存器每 64 秒更新一次，以 60 個 tick 為週期讀取
    static int s_temp_tick = 0;
    if (++s_temp_tick >= 60) {
        s_temp_tick = 0;
        float temp;
        char buf[16];
        if (module_time_get_temp(&temp) == ESP_OK) {
            snprintf(buf, sizeof(buf), "%.1f°C", temp);
            lv_label_set_text(s_lbl_temp_val, buf);
        }
    }
}

// 由 IMU 任務呼叫（LVGL 外部，需加鎖）
static void on_imu_event(imu_event_t event)
{
    if (s_lbl_imu_val == NULL) return;

    lvgl_port_lock(0);
    switch (event) {
        case IMU_EVENT_TAP:
            ESP_LOGI(TAG, "[IMU] 敲擊");
            lv_label_set_text(s_lbl_imu_val, "敲擊");
            lv_obj_set_style_text_color(s_lbl_imu_val, lv_color_white(), 0);
            s_tap_clear_ticks = TAP_CLEAR_TICKS;
            break;
        case IMU_EVENT_SHAKE_START:
            ESP_LOGI(TAG, "[IMU] 晃動開始");
            lv_label_set_text(s_lbl_imu_val, "晃動中");
            lv_obj_set_style_text_color(s_lbl_imu_val, lv_color_white(), 0);
            s_tap_clear_ticks = 0;  // 取消任何進行中的 tap 消退計數
            break;
        case IMU_EVENT_SHAKE_END:
            ESP_LOGI(TAG, "[IMU] 晃動結束");
            lv_label_set_text(s_lbl_imu_val, "—");
            lv_obj_set_style_text_color(s_lbl_imu_val, lv_color_make(180, 180, 180), 0);
            break;
        default:
            break;
    }
    lvgl_port_unlock();
}

// 點擊時間列 → 打開鬧鐘設定 overlay（LVGL 事件回呼，已在 lock 內）
static void on_time_clicked(lv_event_t *e)
{
    (void)e;
    module_ui_alarm_open();
}

// 由 GPIO 中斷 / PIR 任務呼叫（LVGL 外部，需加鎖）
static void on_pir_event(pir_event_t event)
{
    if (s_lbl_pir_status == NULL) return;

    bool detected = (event == PIR_EVENT_DETECTED);

    lvgl_port_lock(0);
    lv_label_set_text(s_lbl_pir_status, detected ? "●" : "○");
    lv_obj_set_style_text_color(s_lbl_pir_status,
        detected ? lv_color_make(100, 210, 100) : lv_color_make(170, 170, 170), 0);
    lvgl_port_unlock();
}

// ======================================================================
// 公開 API
// ======================================================================

void module_ui_main_show(void)
{
    ESP_LOGI(TAG, "建立主畫面");

    lvgl_port_lock(0);

    // ------------------------------------------------------------------
    // 建立獨立螢幕物件（便於日後多畫面切換）
    // ------------------------------------------------------------------
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    // ------------------------------------------------------------------
    // 狀態列（480 × 24px，頂部）
    // ------------------------------------------------------------------
    lv_obj_t *status_bar = lv_obj_create(s_scr);
    lv_obj_set_size(status_bar, CONFIG_ILI9488_WIDTH, STATUS_BAR_H);
    lv_obj_set_pos(status_bar, 0, 0);
    lv_obj_set_style_bg_color(status_bar, lv_color_make(18, 18, 18), 0);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 0, 0);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

    // 年月日週（左）
    s_lbl_date_status = lv_label_create(status_bar);
    lv_label_set_text(s_lbl_date_status, "----年--月--日 ---");
    lv_obj_set_style_text_font(s_lbl_date_status, &font_huninn_14, 0);
    lv_obj_set_style_text_color(s_lbl_date_status, lv_color_make(200, 200, 200), 0);
    lv_obj_align(s_lbl_date_status, LV_ALIGN_LEFT_MID, 10, 0);

    // PIR 指示點（右）
    s_lbl_pir_status = lv_label_create(status_bar);
    lv_label_set_text(s_lbl_pir_status, "○");
    lv_obj_set_style_text_font(s_lbl_pir_status, &font_huninn_14, 0);
    lv_obj_set_style_text_color(s_lbl_pir_status, lv_color_make(170, 170, 170), 0);
    lv_obj_align(s_lbl_pir_status, LV_ALIGN_RIGHT_MID, -10, 0);

    // WiFi icon（PIR 點左側）
    lv_obj_t *lbl_wifi = lv_label_create(status_bar);
    lv_label_set_text(lbl_wifi, MI_WIFI_0);
    lv_obj_set_style_text_font(lbl_wifi, &font_mi_14, 0);
    lv_obj_set_style_text_color(lbl_wifi, lv_color_make(170, 170, 170), 0);
    lv_obj_align_to(lbl_wifi, s_lbl_pir_status, LV_ALIGN_OUT_LEFT_MID, -8, 0);

    // Bluetooth icon（WiFi 左側）
    lv_obj_t *lbl_bt = lv_label_create(status_bar);
    lv_label_set_text(lbl_bt, MI_BLUETOOTH_DISABLED);
    lv_obj_set_style_text_font(lbl_bt, &font_mi_14, 0);
    lv_obj_set_style_text_color(lbl_bt, lv_color_make(170, 170, 170), 0);
    lv_obj_align_to(lbl_bt, lbl_wifi, LV_ALIGN_OUT_LEFT_MID, -4, 0);

    // ------------------------------------------------------------------
    // 左右分隔線（垂直，1px）
    // ------------------------------------------------------------------
    lv_obj_t *divider_v = lv_obj_create(s_scr);
    lv_obj_set_size(divider_v, DIVIDER_W, MAIN_AREA_H);
    lv_obj_set_pos(divider_v, LEFT_PANEL_W, STATUS_BAR_H);
    lv_obj_set_style_bg_color(divider_v, lv_color_make(45, 45, 45), 0);
    lv_obj_set_style_bg_opa(divider_v, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(divider_v, 0, 0);
    lv_obj_set_style_radius(divider_v, 0, 0);
    lv_obj_set_style_pad_all(divider_v, 0, 0);

    // ------------------------------------------------------------------
    // 左側面板（270 × 296px）
    // ------------------------------------------------------------------
    lv_obj_t *left_panel = lv_obj_create(s_scr);
    lv_obj_set_size(left_panel, LEFT_PANEL_W, MAIN_AREA_H);
    lv_obj_set_pos(left_panel, 0, STATUS_BAR_H);
    lv_obj_set_style_bg_opa(left_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left_panel, 0, 0);
    lv_obj_set_style_pad_all(left_panel, 0, 0);
    lv_obj_clear_flag(left_panel, LV_OBJ_FLAG_SCROLLABLE);

    // == 左上：時鐘區（270 × 148px）==================================

    lv_obj_t *clock_area = lv_obj_create(left_panel);
    lv_obj_set_size(clock_area, LEFT_PANEL_W, LEFT_HALF_H);
    lv_obj_set_pos(clock_area, 0, 0);
    lv_obj_set_style_bg_opa(clock_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(clock_area, 0, 0);
    lv_obj_set_style_pad_all(clock_area, 0, 0);
    lv_obj_clear_flag(clock_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(clock_area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(clock_area, on_time_clicked, LV_EVENT_CLICKED, NULL);

    // HH:MM  :SS（Flex Row，底部基線對齊）
    lv_obj_t *time_row = lv_obj_create(clock_area);
    lv_obj_set_size(time_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_layout(time_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(time_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(time_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(time_row, 0, 0);
    lv_obj_set_style_pad_all(time_row, 0, 0);
    lv_obj_set_style_pad_column(time_row, 2, 0);
    lv_obj_clear_flag(time_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(time_row, LV_ALIGN_CENTER, 0, -14);

    s_lbl_time = lv_label_create(time_row);
    lv_label_set_text(s_lbl_time, "--:--");
    lv_obj_set_style_text_font(s_lbl_time, &font_huninn_48, 0);
    lv_obj_set_style_text_color(s_lbl_time, lv_color_white(), 0);

    s_lbl_sec = lv_label_create(time_row);
    lv_label_set_text(s_lbl_sec, ":--");
    lv_obj_set_style_text_font(s_lbl_sec, &font_huninn_48, 0);
    lv_obj_set_style_text_color(s_lbl_sec, lv_color_make(190, 190, 190), 0);

    // 下一個鬧鐘提示
    s_lbl_alarm = lv_label_create(clock_area);
    lv_label_set_text(s_lbl_alarm, "您目前沒有下一個鬧鐘");
    lv_obj_set_style_text_font(s_lbl_alarm, &font_huninn_14, 0);
    lv_obj_set_style_text_color(s_lbl_alarm, lv_color_make(170, 170, 170), 0);
    lv_obj_align(s_lbl_alarm, LV_ALIGN_CENTER, 0, 40);

    // 上下分隔線（水平，1px）
    lv_obj_t *divider_h = lv_obj_create(left_panel);
    lv_obj_set_size(divider_h, LEFT_PANEL_W, DIVIDER_W);
    lv_obj_set_pos(divider_h, 0, LEFT_HALF_H);
    lv_obj_set_style_bg_color(divider_h, lv_color_make(45, 45, 45), 0);
    lv_obj_set_style_bg_opa(divider_h, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(divider_h, 0, 0);
    lv_obj_set_style_radius(divider_h, 0, 0);
    lv_obj_set_style_pad_all(divider_h, 0, 0);

    // == 左下：IMU 卡片區（270 × 148px）==============================

    lv_obj_t *imu_area = lv_obj_create(left_panel);
    lv_obj_set_size(imu_area, LEFT_PANEL_W, LEFT_HALF_H);
    lv_obj_set_pos(imu_area, 0, LEFT_HALF_H + DIVIDER_W);
    lv_obj_set_style_bg_opa(imu_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(imu_area, 0, 0);
    lv_obj_set_style_pad_all(imu_area, 0, 0);
    lv_obj_clear_flag(imu_area, LV_OBJ_FLAG_SCROLLABLE);

    // 單一 IMU 卡片（居中）
    int imu_card_x = (LEFT_PANEL_W - IMU_CARD_W) / 2;
    int imu_card_y = (LEFT_HALF_H - IMU_CARD_H) / 2;

    lv_obj_t *card_imu = lv_obj_create(imu_area);
    lv_obj_set_size(card_imu, IMU_CARD_W, IMU_CARD_H);
    lv_obj_set_pos(card_imu, imu_card_x, imu_card_y);
    lv_obj_set_style_bg_color(card_imu, lv_color_make(25, 25, 25), 0);
    lv_obj_set_style_bg_opa(card_imu, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card_imu, lv_color_make(55, 55, 55), 0);
    lv_obj_set_style_border_width(card_imu, 1, 0);
    lv_obj_set_style_radius(card_imu, 8, 0);
    lv_obj_set_style_pad_all(card_imu, 8, 0);
    lv_obj_clear_flag(card_imu, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_imu_title = lv_label_create(card_imu);
    lv_label_set_text(lbl_imu_title, "動作偵測");
    lv_obj_set_style_text_font(lbl_imu_title, &font_huninn_14, 0);
    lv_obj_set_style_text_color(lbl_imu_title, lv_color_make(190, 190, 190), 0);
    lv_obj_align(lbl_imu_title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_lbl_imu_val = lv_label_create(card_imu);
    lv_label_set_text(s_lbl_imu_val, "—");
    lv_obj_set_style_text_font(s_lbl_imu_val, &font_huninn_24, 0);
    lv_obj_set_style_text_color(s_lbl_imu_val, lv_color_make(180, 180, 180), 0);
    lv_obj_align(s_lbl_imu_val, LV_ALIGN_CENTER, 0, 8);

    // ------------------------------------------------------------------
    // 右側資訊面板（209 × 296px）
    // ------------------------------------------------------------------
    lv_obj_t *right_panel = lv_obj_create(s_scr);
    lv_obj_set_size(right_panel, RIGHT_PANEL_W, MAIN_AREA_H);
    lv_obj_set_pos(right_panel, LEFT_PANEL_W + DIVIDER_W, STATUS_BAR_H);
    lv_obj_set_style_bg_opa(right_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_panel, 0, 0);
    lv_obj_set_style_pad_all(right_panel, 8, 0);
    lv_obj_set_style_pad_top(right_panel, 0, 0);
    lv_obj_clear_flag(right_panel, LV_OBJ_FLAG_SCROLLABLE);

    // 室內氣溫卡片
    lv_obj_t *card_temp = create_right_card(right_panel, "室內氣溫", 12);
    s_lbl_temp_val = lv_label_create(card_temp);
    lv_label_set_text(s_lbl_temp_val, "--.-°C");
    lv_obj_set_style_text_font(s_lbl_temp_val, &font_huninn_24, 0);
    lv_obj_set_style_text_color(s_lbl_temp_val, lv_color_white(), 0);
    lv_obj_align(s_lbl_temp_val, LV_ALIGN_CENTER, 0, 8);

    // 室外氣溫卡片（待 WiFi 接上後更新）
    lv_obj_t *card_temp_out = create_right_card(right_panel, "室外氣溫", 12 + CARD_H + CARD_GAP);

    lv_obj_t *lbl_temp_out_loc = lv_label_create(card_temp_out);
    lv_label_set_text(lbl_temp_out_loc, "桃園市平鎮區");
    lv_obj_set_style_text_font(lbl_temp_out_loc, &font_huninn_14, 0);
    lv_obj_set_style_text_color(lbl_temp_out_loc, lv_color_make(170, 170, 170), 0);
    lv_obj_align(lbl_temp_out_loc, LV_ALIGN_TOP_RIGHT, 0, 0);

    lv_obj_t *lbl_temp_out = lv_label_create(card_temp_out);
    lv_label_set_text(lbl_temp_out, "請連接 WiFi");
    lv_obj_set_style_text_font(lbl_temp_out, &font_huninn_18, 0);
    lv_obj_set_style_text_color(lbl_temp_out, lv_color_white(), 0);
    lv_obj_align(lbl_temp_out, LV_ALIGN_CENTER, 0, 8);

    // ------------------------------------------------------------------
    // 初始資料填入
    // ------------------------------------------------------------------
    refresh_clock();

    float temp;
    if (module_time_get_temp(&temp) == ESP_OK) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f°C", temp);
        lv_label_set_text(s_lbl_temp_val, buf);
    }

    bool pir_detected = module_pir_is_detected();
    lv_label_set_text(s_lbl_pir_status, pir_detected ? "●" : "○");
    lv_obj_set_style_text_color(s_lbl_pir_status,
        pir_detected ? lv_color_make(100, 210, 100) : lv_color_make(170, 170, 170), 0);

    // ------------------------------------------------------------------
    // 切換至主畫面（淡入 400ms）
    // ------------------------------------------------------------------
    // auto_del=true：LVGL 在動畫結束時釋放前一個 screen（網路設定畫面）
    lv_screen_load_anim(s_scr, LV_SCR_LOAD_ANIM_FADE_IN, 400, 0, true);

    s_clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);

    lvgl_port_unlock();

    module_imu_add_callback(on_imu_event);
    module_pir_add_callback(on_pir_event);

    ESP_LOGI(TAG, "主畫面就緒");
}

void module_ui_main_destroy(void)
{
    module_imu_remove_callback(on_imu_event);
    module_pir_remove_callback(on_pir_event);

    lvgl_port_lock(0);

    if (s_clock_timer) {
        lv_timer_delete(s_clock_timer);
        s_clock_timer = NULL;
    }

    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
    }

    lvgl_port_unlock();

    s_lbl_date_status = NULL;
    s_lbl_pir_status  = NULL;
    s_lbl_time        = NULL;
    s_lbl_sec         = NULL;
    s_lbl_alarm       = NULL;
    s_lbl_imu_val     = NULL;
    s_lbl_temp_val    = NULL;
    s_tap_clear_ticks = 0;
}
