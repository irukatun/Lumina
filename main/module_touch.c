// ESP-IDF 系統組件
#include <esp_log.h>
#include <esp_err.h>
#include <driver/gpio.h>
#include <esp_lcd_panel_io.h>

// 第三方組件
#include <esp_lcd_touch.h>
#include <esp_lcd_touch_xpt2046.h>

// 本專案模組
#include "board.h"
#include "module_touch.h"

// 日誌標籤
static const char *TAG = "M_Touch";

// ======================================================================
// 私有巨集
// ======================================================================
// 座標校準（需要根據不同螢幕實際情況修改）
#define TOUCH_CAL_X_MIN         -32        // X 軸校準最小 ADC 值（實測校準）
#define TOUCH_CAL_X_MAX         4006       // X 軸校準最大 ADC 值（實測校準）
#define TOUCH_CAL_Y_MIN         170        // Y 軸校準最小 ADC 值（實測值）
#define TOUCH_CAL_Y_MAX         3840       // Y 軸校準最大 ADC 值（實測值）

// ======================================================================
// 私有變數
// ======================================================================
static esp_lcd_panel_io_handle_t s_io_handle    = NULL;
static esp_lcd_touch_handle_t    s_touch_handle = NULL;

// ======================================================================
// 私有函式前向宣告
// ======================================================================
static void touch_calibrate(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num);
static esp_err_t io_init(void);
static esp_err_t xpt2046_init(void);

// ======================================================================
// 私有函式實作
// ======================================================================
// 壓力過濾與座標校準，由 esp_lcd_touch 驅動在讀取後呼叫
static void touch_calibrate(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{
    for (uint8_t i = 0; i < *point_num; i++) {
        // 還原近似原始 ADC 值（驅動已線性映射至 [0, x_max]）
        uint32_t raw_x = (uint32_t)x[i] * 4096 / tp->config.x_max;
        uint32_t raw_y = (uint32_t)y[i] * 4096 / tp->config.y_max;

        // 校準映射：[CAL_MIN, CAL_MAX] → [0, x_max / y_max]
        int32_t cal_x = ((int32_t)raw_x - TOUCH_CAL_X_MIN) * tp->config.x_max / (TOUCH_CAL_X_MAX - TOUCH_CAL_X_MIN);
        int32_t cal_y = ((int32_t)raw_y - TOUCH_CAL_Y_MIN) * tp->config.y_max / (TOUCH_CAL_Y_MAX - TOUCH_CAL_Y_MIN);

        // 邊界限制
        if (cal_x < 0) cal_x = 0;
        if (cal_x > tp->config.x_max) cal_x = tp->config.x_max;
        if (cal_y < 0) cal_y = 0;
        if (cal_y > tp->config.y_max) cal_y = tp->config.y_max;

        x[i] = (uint16_t)cal_x;
        y[i] = (uint16_t)cal_y;
    }
}

// 建立 XPT2046 與 SPI2 溝通的 IO 橋樑
static esp_err_t io_init(void)
{
    const esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num         = CONFIG_GPIO_XPT2046_CS,
        .dc_gpio_num         = GPIO_NUM_NC,  // XPT2046 無 D/C 腳
        .spi_mode            = 0,
        .pclk_hz             = 2000000,    // 2 MHz，XPT2046 DCLK 上限
        .trans_queue_depth   = 3,          // 一次讀取涉及 X、Y、壓力共 3 筆
        .on_color_trans_done = NULL,
        .user_ctx            = NULL,
        .lcd_cmd_bits        = 8,
        .lcd_param_bits      = 8,
    };
    esp_err_t ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &s_io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "觸控 IO 建立失敗: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

// 初始化 XPT2046 觸控驅動
static esp_err_t xpt2046_init(void)
{
    const esp_lcd_touch_config_t touch_cfg = {
        .x_max               = CONFIG_ILI9488_HEIGHT,  // swap_xy 後 x 對應高度
        .y_max               = CONFIG_ILI9488_WIDTH,   // swap_xy 後 y 對應寬度
        .rst_gpio_num        = GPIO_NUM_NC,
        .int_gpio_num        = GPIO_NUM_NC,
        .levels              = { .reset = 0, .interrupt = 0 },
        .flags               = { .swap_xy = 1, .mirror_x = 1, .mirror_y = 1 },
        .process_coordinates = touch_calibrate,
        .interrupt_callback  = NULL,
    };
    esp_err_t ret = esp_lcd_touch_new_spi_xpt2046(s_io_handle, &touch_cfg, &s_touch_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "XPT2046 初始化失敗: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

// ======================================================================
// 公開 API
// ======================================================================
// 初始化 XPT2046 觸控模組
esp_err_t module_touch_init(void)
{
    ESP_LOGI(TAG, "觸控正在初始化");

    esp_err_t ret = io_init();
    if (ret != ESP_OK) return ret;

    ret = xpt2046_init();
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "觸控初始化完成");
    return ESP_OK;
}

// 取得觸控 handle 供 module_lvgl 使用
esp_lcd_touch_handle_t module_touch_handle(void)
{
    return s_touch_handle;
}
