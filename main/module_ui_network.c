// ESP-IDF 系統組件
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <esp_system.h>

// 第三方組件
#include <lvgl.h>
#include <esp_lvgl_port.h>

// 本專案模組
#include "board.h"
#include "fonts/fonts.h"
#include "module_network.h"
#include "module_ui_network.h"

// 日誌標籤
static const char *TAG = "M_UI_Net";

// ======================================================================
// 私有巨集
// ======================================================================

#define BTN_W  360
#define BTN_H   52
#define CX     (CONFIG_ILI9488_WIDTH  / 2)

#define STEP_WIFI     0
#define STEP_INTERNET 1
#define STEP_SERVER   2

// ======================================================================
// 私有型別
// ======================================================================

typedef enum { STEP_PENDING, STEP_RUNNING, STEP_OK, STEP_FAIL } step_state_t;

typedef enum { UI_NET_SELECT, UI_NET_CONNECTING, UI_NET_PROV } ui_net_state_t;

// ======================================================================
// 私有變數
// ======================================================================

static lv_obj_t          *s_scr            = NULL;
static lv_obj_t          *s_panel_select   = NULL;
static lv_obj_t          *s_panel_connect  = NULL;
static lv_obj_t          *s_panel_prov     = NULL;
static SemaphoreHandle_t  s_done_sem       = NULL;
static ui_net_state_t     s_state          = UI_NET_SELECT;

// 連線面板
static lv_obj_t   *s_lbl_conn_title = NULL;
static lv_obj_t   *s_step_icon[3]   = {};
static lv_obj_t   *s_lbl_error      = NULL;
static lv_obj_t   *s_btns_fail      = NULL;
static lv_timer_t *s_server_timer   = NULL;

// 配網面板
static lv_obj_t   *s_lbl_prov_status  = NULL;
static lv_obj_t   *s_btn_prov_restart = NULL;

// ======================================================================
// 私有函式前向宣告
// ======================================================================

static lv_obj_t *create_btn(lv_obj_t *parent, const char *text, bool primary,
                              lv_coord_t offset_y, lv_event_cb_t cb);
static void      build_panel_select(void);
static void      build_panel_connect(void);
static void      build_panel_prov(void);
static void      show_panel(lv_obj_t *panel);
static void      set_step(int idx, step_state_t state);
static void      reset_steps(void);
static void      show_fail_state(const char *msg);
static void      finish(void);
static void      on_net_event(module_network_event_t event);
static void      on_prov_event(module_network_prov_event_t event);

// ======================================================================
// 私有函式實作 — 通用輔助
// ======================================================================

static void finish(void)
{
    if (s_done_sem) xSemaphoreGive(s_done_sem);
}

static void show_panel(lv_obj_t *panel)
{
    if (s_panel_select)  lv_obj_add_flag(s_panel_select,  LV_OBJ_FLAG_HIDDEN);
    if (s_panel_connect) lv_obj_add_flag(s_panel_connect, LV_OBJ_FLAG_HIDDEN);
    if (s_panel_prov)    lv_obj_add_flag(s_panel_prov,    LV_OBJ_FLAG_HIDDEN);
    if (panel)           lv_obj_clear_flag(panel,          LV_OBJ_FLAG_HIDDEN);
}

static void set_step(int idx, step_state_t state)
{
    if (!s_step_icon[idx]) return;
    switch (state) {
        case STEP_PENDING:
            lv_label_set_text(s_step_icon[idx], LV_SYMBOL_MINUS);
            lv_obj_set_style_text_color(s_step_icon[idx], lv_color_make(70, 70, 70), 0);
            break;
        case STEP_RUNNING:
            lv_label_set_text(s_step_icon[idx], LV_SYMBOL_REFRESH);
            lv_obj_set_style_text_color(s_step_icon[idx], lv_color_make(200, 200, 200), 0);
            break;
        case STEP_OK:
            lv_label_set_text(s_step_icon[idx], LV_SYMBOL_OK);
            lv_obj_set_style_text_color(s_step_icon[idx], lv_color_make(100, 210, 100), 0);
            break;
        case STEP_FAIL:
            lv_label_set_text(s_step_icon[idx], LV_SYMBOL_CLOSE);
            lv_obj_set_style_text_color(s_step_icon[idx], lv_color_make(224, 80, 80), 0);
            break;
    }
}

static void reset_steps(void)
{
    for (int i = 0; i < 3; i++) set_step(i, STEP_PENDING);
    lv_obj_add_flag(s_lbl_error,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_btns_fail,  LV_OBJ_FLAG_HIDDEN);
}

static void show_fail_state(const char *msg)
{
    lv_label_set_text(s_lbl_conn_title, "無法連線");
    lv_obj_set_style_text_color(s_lbl_conn_title, lv_color_make(224, 80, 80), 0);
    lv_label_set_text(s_lbl_error, msg);
    lv_obj_clear_flag(s_lbl_error, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_btns_fail, LV_OBJ_FLAG_HIDDEN);
}

// ======================================================================
// 私有函式實作 — 建立 UI 元件
// ======================================================================

static lv_obj_t *create_btn(lv_obj_t *parent, const char *text, bool primary,
                              lv_coord_t offset_y, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_set_size(btn, BTN_W, BTN_H);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, offset_y);
    lv_obj_set_style_bg_color(btn,
        primary ? lv_color_white() : lv_color_make(40, 40, 40), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &font_huninn_18, 0);
    lv_obj_set_style_text_color(lbl,
        primary ? lv_color_black() : lv_color_white(), 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    return btn;
}

// 在 connect 面板建立一列步驟列（icon + 說明文字）
static void create_step_row(lv_obj_t *parent, int idx,
                             const char *label_text, lv_coord_t offset_y)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, BTN_W, 32);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, offset_y);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    // icon 使用 LVGL 內建符號字體（不設字體，使用系統預設）
    s_step_icon[idx] = lv_label_create(row);
    lv_label_set_text(s_step_icon[idx], LV_SYMBOL_MINUS);
    lv_obj_set_style_text_color(s_step_icon[idx], lv_color_make(70, 70, 70), 0);
    lv_obj_align(s_step_icon[idx], LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_font(lbl, &font_huninn_18, 0);
    lv_obj_set_style_text_color(lbl, lv_color_make(200, 200, 200), 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 28, 0);
}

// 在 fail 按鈕列建立小按鈕
static lv_obj_t *create_small_btn(lv_obj_t *parent, const char *text,
                                   lv_color_t bg, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, 44);
    lv_obj_set_style_pad_hor(btn, 16, 0);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_pad_ver(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &font_huninn_18, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    return btn;
}

// ======================================================================
// 私有函式實作 — 按鈕事件（LVGL task，已在 lock 內）
// ======================================================================

static void on_btn_use_saved(lv_event_t *e)
{
    (void)e;
    s_state = UI_NET_CONNECTING;
    show_panel(s_panel_connect);
    reset_steps();
    set_step(STEP_WIFI, STEP_RUNNING);
    lv_label_set_text(s_lbl_conn_title, "正在連線...");
    lv_obj_set_style_text_color(s_lbl_conn_title, lv_color_white(), 0);
    module_network_connect();
}

static void on_btn_other_network(lv_event_t *e)
{
    (void)e;
    s_state = UI_NET_PROV;
    lv_label_set_text(s_lbl_prov_status, "等待手機連線中...");
    lv_obj_set_style_text_color(s_lbl_prov_status, lv_color_make(200, 200, 200), 0);
    lv_obj_add_flag(s_btn_prov_restart, LV_OBJ_FLAG_HIDDEN);
    show_panel(s_panel_prov);
    module_network_provision_start();
}

static void on_btn_skip(lv_event_t *e)
{
    (void)e;
    finish();
}

static void on_btn_retry(lv_event_t *e)
{
    (void)e;
    reset_steps();
    set_step(STEP_WIFI, STEP_RUNNING);
    lv_label_set_text(s_lbl_conn_title, "正在連線...");
    lv_obj_set_style_text_color(s_lbl_conn_title, lv_color_white(), 0);
    module_network_connect();
}

static void on_btn_reprov(lv_event_t *e)
{
    (void)e;
    show_panel(s_panel_select);
    s_state = UI_NET_SELECT;
}

static void on_btn_prov_restart(lv_event_t *e)
{
    (void)e;
    esp_restart();
}


// ======================================================================
// 私有函式實作 — 建立面板
// ======================================================================

static void build_panel_select(void)
{
    s_panel_select = lv_obj_create(s_scr);
    lv_obj_set_size(s_panel_select, CONFIG_ILI9488_WIDTH, CONFIG_ILI9488_HEIGHT);
    lv_obj_set_pos(s_panel_select, 0, 0);
    lv_obj_set_style_bg_opa(s_panel_select, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_panel_select, 0, 0);
    lv_obj_set_style_pad_all(s_panel_select, 0, 0);
    lv_obj_clear_flag(s_panel_select, LV_OBJ_FLAG_SCROLLABLE);

    // 標題
    lv_obj_t *lbl_title = lv_label_create(s_panel_select);
    lv_label_set_text(lbl_title, "網路設定");
    lv_obj_set_style_text_font(lbl_title, &font_huninn_24, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, -110);

    // 副標題
    lv_obj_t *lbl_sub = lv_label_create(s_panel_select);
    lv_label_set_text(lbl_sub, "連接 WiFi 以使用完整功能");
    lv_obj_set_style_text_font(lbl_sub, &font_huninn_14, 0);
    lv_obj_set_style_text_color(lbl_sub, lv_color_make(150, 150, 150), 0);
    lv_obj_align(lbl_sub, LV_ALIGN_CENTER, 0, -75);

    // 按鈕 1：使用上次的連線（無憑證時灰掉不可點）
    bool has_creds = module_network_is_provisioned();
    lv_obj_t *btn1 = create_btn(s_panel_select,
                                has_creds ? "使用上次的連線" : "無已儲存的網路",
                                true, -18,
                                has_creds ? on_btn_use_saved : NULL);
    if (!has_creds) {
        lv_obj_set_style_bg_color(btn1, lv_color_make(35, 35, 35), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(btn1, 0), lv_color_make(80, 80, 80), 0);
    }

    // 按鈕 2：連線到其他網路（配網）
    create_btn(s_panel_select, "連線到其他網路", false, +50, on_btn_other_network);

    // 跳過（純文字按鈕）
    lv_obj_t *btn_skip = lv_obj_create(s_panel_select);
    lv_obj_set_size(btn_skip, 120, 30);
    lv_obj_set_style_bg_opa(btn_skip, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_skip, 0, 0);
    lv_obj_set_style_pad_all(btn_skip, 0, 0);
    lv_obj_align(btn_skip, LV_ALIGN_CENTER, 0, +122);
    lv_obj_add_event_cb(btn_skip, on_btn_skip, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_skip = lv_label_create(btn_skip);
    lv_label_set_text(lbl_skip, "跳過，稍後再說");
    lv_obj_set_style_text_font(lbl_skip, &font_huninn_14, 0);
    lv_obj_set_style_text_color(lbl_skip, lv_color_make(120, 120, 120), 0);
    lv_obj_align(lbl_skip, LV_ALIGN_CENTER, 0, 0);
}

static void build_panel_connect(void)
{
    s_panel_connect = lv_obj_create(s_scr);
    lv_obj_set_size(s_panel_connect, CONFIG_ILI9488_WIDTH, CONFIG_ILI9488_HEIGHT);
    lv_obj_set_pos(s_panel_connect, 0, 0);
    lv_obj_set_style_bg_opa(s_panel_connect, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_panel_connect, 0, 0);
    lv_obj_set_style_pad_all(s_panel_connect, 0, 0);
    lv_obj_clear_flag(s_panel_connect, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_panel_connect, LV_OBJ_FLAG_HIDDEN);

    // 標題
    s_lbl_conn_title = lv_label_create(s_panel_connect);
    lv_label_set_text(s_lbl_conn_title, "正在連線...");
    lv_obj_set_style_text_font(s_lbl_conn_title, &font_huninn_24, 0);
    lv_obj_set_style_text_color(s_lbl_conn_title, lv_color_white(), 0);
    lv_obj_align(s_lbl_conn_title, LV_ALIGN_CENTER, 0, -100);

    // 步驟列（3 列，各間隔 38px）
    create_step_row(s_panel_connect, STEP_WIFI,     "連接 WiFi",   -40);
    create_step_row(s_panel_connect, STEP_INTERNET, "確認網路連線", -2);
    create_step_row(s_panel_connect, STEP_SERVER,   "連接小鹿米服務", +36);

    // 錯誤說明（失敗時顯示）
    s_lbl_error = lv_label_create(s_panel_connect);
    lv_label_set_text(s_lbl_error, "");
    lv_obj_set_style_text_font(s_lbl_error, &font_huninn_14, 0);
    lv_obj_set_style_text_color(s_lbl_error, lv_color_make(200, 100, 100), 0);
    lv_obj_align(s_lbl_error, LV_ALIGN_CENTER, 0, +82);
    lv_obj_add_flag(s_lbl_error, LV_OBJ_FLAG_HIDDEN);

    // 失敗時的操作按鈕列
    s_btns_fail = lv_obj_create(s_panel_connect);
    lv_obj_set_size(s_btns_fail, BTN_W, 50);
    lv_obj_align(s_btns_fail, LV_ALIGN_CENTER, 0, +120);
    lv_obj_set_style_bg_opa(s_btns_fail, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_btns_fail, 0, 0);
    lv_obj_set_style_pad_all(s_btns_fail, 0, 0);
    lv_obj_set_layout(s_btns_fail, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(s_btns_fail, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_btns_fail, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(s_btns_fail, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_btns_fail, LV_OBJ_FLAG_HIDDEN);

    create_small_btn(s_btns_fail, "重試",     lv_color_make(50, 50, 50), on_btn_retry);
    create_small_btn(s_btns_fail, "重新配網", lv_color_make(50, 50, 50), on_btn_reprov);
    create_small_btn(s_btns_fail, "跳過",     lv_color_make(35, 35, 35), on_btn_skip);
}

static void build_panel_prov(void)
{
    s_panel_prov = lv_obj_create(s_scr);
    lv_obj_set_size(s_panel_prov, CONFIG_ILI9488_WIDTH, CONFIG_ILI9488_HEIGHT);
    lv_obj_set_pos(s_panel_prov, 0, 0);
    lv_obj_set_style_bg_opa(s_panel_prov, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_panel_prov, 0, 0);
    lv_obj_set_style_pad_all(s_panel_prov, 0, 0);
    lv_obj_clear_flag(s_panel_prov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_panel_prov, LV_OBJ_FLAG_HIDDEN);

    // 標題
    lv_obj_t *lbl_title = lv_label_create(s_panel_prov);
    lv_label_set_text(lbl_title, "BLE 配網");
    lv_obj_set_style_text_font(lbl_title, &font_huninn_24, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, -100);

    // 裝置名稱
    lv_obj_t *lbl_dev = lv_label_create(s_panel_prov);
    lv_label_set_text(lbl_dev, "裝置名稱：小鹿米智慧桌上助理");
    lv_obj_set_style_text_font(lbl_dev, &font_huninn_18, 0);
    lv_obj_set_style_text_color(lbl_dev, lv_color_make(200, 200, 200), 0);
    lv_obj_align(lbl_dev, LV_ALIGN_CENTER, 0, -52);

    // 操作說明
    lv_obj_t *lbl_inst = lv_label_create(s_panel_prov);
    lv_label_set_text(lbl_inst, "請使用 ESP BLE Prov 應用程式進行配對");
    lv_obj_set_style_text_font(lbl_inst, &font_huninn_14, 0);
    lv_obj_set_style_text_color(lbl_inst, lv_color_make(150, 150, 150), 0);
    lv_obj_align(lbl_inst, LV_ALIGN_CENTER, 0, -14);

    // 配對碼
    lv_obj_t *lbl_pin = lv_label_create(s_panel_prov);
    lv_label_set_text(lbl_pin, "配對碼：lumina");
    lv_obj_set_style_text_font(lbl_pin, &font_huninn_14, 0);
    lv_obj_set_style_text_color(lbl_pin, lv_color_make(150, 150, 150), 0);
    lv_obj_align(lbl_pin, LV_ALIGN_CENTER, 0, +12);

    // 狀態文字
    s_lbl_prov_status = lv_label_create(s_panel_prov);
    lv_label_set_text(s_lbl_prov_status, "等待手機連線中...");
    lv_obj_set_style_text_font(s_lbl_prov_status, &font_huninn_18, 0);
    lv_obj_set_style_text_color(s_lbl_prov_status, lv_color_make(200, 200, 200), 0);
    lv_obj_set_style_text_align(s_lbl_prov_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_lbl_prov_status, LV_ALIGN_CENTER, 0, +50);

    // 重新啟動按鈕（只在 CRED_FAIL 時顯示）
    s_btn_prov_restart = create_btn(s_panel_prov, "重新啟動", false, +112, on_btn_prov_restart);
    lv_obj_add_flag(s_btn_prov_restart, LV_OBJ_FLAG_HIDDEN);
}

// ======================================================================
// 私有函式實作 — 模組事件回呼（外部 task，需加鎖）
// ======================================================================

static void server_check_done_cb(lv_timer_t *t)
{
    lv_timer_delete(t);
    s_server_timer = NULL;
    set_step(STEP_SERVER, STEP_OK);
    lv_label_set_text(s_lbl_conn_title, "連線成功");
    lv_obj_set_style_text_color(s_lbl_conn_title, lv_color_make(100, 210, 100), 0);
    // 短暫停留讓用戶看到成功，再進入主畫面
    finish();
}

static void on_net_event(module_network_event_t event)
{
    lvgl_port_lock(0);

    switch (event) {
        case NET_EVT_CONNECTED:
            if (s_state == UI_NET_CONNECTING || s_state == UI_NET_PROV) {
                s_state = UI_NET_CONNECTING;
                show_panel(s_panel_connect);
                reset_steps();
                set_step(STEP_WIFI, STEP_OK);
                set_step(STEP_INTERNET, STEP_RUNNING);
                lv_label_set_text(s_lbl_conn_title, "正在確認網路...");
                lv_obj_set_style_text_color(s_lbl_conn_title, lv_color_white(), 0);
            }
            break;

        case NET_EVT_CONNECT_FAILED:
            if (s_state == UI_NET_CONNECTING) {
                set_step(STEP_WIFI, STEP_FAIL);
                show_fail_state("無法連接到 WiFi，請確認密碼是否正確");
            }
            break;

        case NET_EVT_INTERNET_UP:
            if (s_state != UI_NET_CONNECTING) break;
            set_step(STEP_INTERNET, STEP_OK);
            set_step(STEP_SERVER, STEP_RUNNING);
            lv_label_set_text(s_lbl_conn_title, "正在連接服務...");
            // 伺服器假等待（1.5 秒後標記完成）
            s_server_timer = lv_timer_create(server_check_done_cb, 1500, NULL);
            lv_timer_set_repeat_count(s_server_timer, 1);
            break;

        case NET_EVT_INTERNET_DOWN:
            if (s_state != UI_NET_CONNECTING) break;
            set_step(STEP_INTERNET, STEP_FAIL);
            show_fail_state("WiFi 已連線，但無法存取網際網路");
            break;

        default:
            break;
    }

    lvgl_port_unlock();
}

static void on_prov_event(module_network_prov_event_t event)
{
    lvgl_port_lock(0);

    switch (event) {
        case NET_PROV_EVT_STARTED:
            lv_label_set_text(s_lbl_prov_status, "等待手機連線中...");
            lv_obj_set_style_text_color(s_lbl_prov_status, lv_color_make(200, 200, 200), 0);
            break;

        case NET_PROV_EVT_CRED_RECV:
            lv_label_set_text(s_lbl_prov_status, "正在驗證憑證...");
            lv_obj_set_style_text_color(s_lbl_prov_status, lv_color_make(200, 200, 200), 0);
            break;

        case NET_PROV_EVT_CRED_FAIL:
            lv_label_set_text(s_lbl_prov_status, "密碼錯誤\n請在應用程式中重新輸入或按下方按鈕重新啟動");
            lv_obj_set_style_text_color(s_lbl_prov_status, lv_color_make(224, 80, 80), 0);
            lv_obj_clear_flag(s_btn_prov_restart, LV_OBJ_FLAG_HIDDEN);
            break;

        case NET_PROV_EVT_DONE:
            // NET_EVT_CONNECTED 會先到，on_net_event 已切換到 connect 面板
            break;
    }

    lvgl_port_unlock();
}

// ======================================================================
// 公開 API
// ======================================================================

void module_ui_network_show(void)
{
    ESP_LOGI(TAG, "建立網路設定畫面");

    s_done_sem = xSemaphoreCreateBinary();
    s_state    = UI_NET_SELECT;

    lvgl_port_lock(0);

    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    build_panel_select();
    build_panel_connect();
    build_panel_prov();

    // auto_del=true：LVGL 在動畫結束時釋放前一個 screen（開機畫面）
    lv_screen_load_anim(s_scr, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, true);

    lvgl_port_unlock();

    module_network_add_event_callback(on_net_event);
    module_network_add_prov_callback(on_prov_event);

    // 阻塞直到用戶完成選擇或跳過
    xSemaphoreTake(s_done_sem, portMAX_DELAY);

    module_network_remove_event_callback(on_net_event);
    module_network_remove_prov_callback(on_prov_event);

    if (s_server_timer) {
        lvgl_port_lock(0);
        lv_timer_delete(s_server_timer);
        s_server_timer = NULL;
        lvgl_port_unlock();
    }

    vSemaphoreDelete(s_done_sem);
    s_done_sem = NULL;

    module_network_release_prov();
    ESP_LOGI(TAG, "網路設定完成");
}
