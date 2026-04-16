// ESP-IDF 系統組件
#include <esp_log.h>
#include <esp_err.h>

// 第三方組件
#include <esp_lvgl_port.h>

// 本專案模組
#include "board.h"
#include "module_display.h"
#include "module_touch.h"
#include "module_lvgl.h"

// 日誌標籤
static const char *TAG = "M_LVGL";

// ======================================================================
// 私有巨集
// ======================================================================
#define LVGL_TICK_PERIOD_MS    2

// ======================================================================
// 私有變數
// ======================================================================
static lv_display_t *s_lvgl_display  = NULL;
static lv_indev_t   *s_touch_indev   = NULL;

// ======================================================================
// 公開 API
// ======================================================================
// 初始化 LVGL 並整合顯示器與觸控輸入
esp_err_t module_lvgl_init(void)
{
    ESP_LOGI(TAG, "LVGL 正在初始化");

    // 步驟 1：初始化 esp_lvgl_port，建立 LVGL task 與 tick timer
    const lvgl_port_cfg_t port_cfg = {
        .task_priority     = 4,
        .task_stack        = 8192,
        .task_affinity     = -1,                 // 不綁定核心
        .task_max_sleep_ms = 500,
        .timer_period_ms   = LVGL_TICK_PERIOD_MS,
    };
    esp_err_t ret = lvgl_port_init(&port_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化 LVGL 執行環境失敗: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "LVGL 正在連接至 ILI9488");
    // 步驟 2：建立 LVGL display，連接 ILI9488 panel
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle     = module_display_io_handle(),
        .panel_handle  = module_display_panel_handle(),
        .buffer_size   = CONFIG_ILI9488_WIDTH * 16,  // 對齊 SPI 單次最大傳輸（480×16×RGB888=23040 bytes）
        .double_buffer = true,
        .hres          = CONFIG_ILI9488_WIDTH,
        .vres          = CONFIG_ILI9488_HEIGHT,
        .monochrome    = false,
        .color_format  = LV_COLOR_FORMAT_RGB565,
        .rotation      = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },  // 已在 panel_init 設定
        .flags         = { .buff_dma = false },
    };
    s_lvgl_display = lvgl_port_add_disp(&disp_cfg);
    if (s_lvgl_display == NULL) {
        ESP_LOGE(TAG, "連接顯示器至 LVGL 失敗");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LVGL 正在連接至 XPT2046");
    // 步驟 3：建立 LVGL 輸入裝置，連接 XPT2046 觸控
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp   = s_lvgl_display,
        .handle = module_touch_handle(),
    };
    s_touch_indev = lvgl_port_add_touch(&touch_cfg);
    if (s_touch_indev == NULL) {
        ESP_LOGE(TAG, "連接觸控至 LVGL 失敗");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LVGL 初始化完成");
    return ESP_OK;
}
