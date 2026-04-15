// ESP-IDF 系統組件
#include <esp_log.h>
#include <esp_err.h>
#include <driver/ledc.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

// 第三方組件
#include <esp_lcd_ili9488.h>

// 本專案模組
#include "board.h"
#include "module_display.h"

// 日誌標籤
static const char *TAG = "M_Display";

// ======================================================================
// 私有變數
// ======================================================================
static esp_lcd_panel_io_handle_t s_io_handle    = NULL;
static esp_lcd_panel_handle_t    s_panel_handle = NULL;

// ======================================================================
// 私有函式前向宣告
// ======================================================================
static esp_err_t backlight_init(void);
static esp_err_t panel_init(void);

// ======================================================================
// 私有函式實作
// ======================================================================
// 初始化 LEDC PWM 背光，初始亮度為 0
static esp_err_t backlight_init(void)
{
    const ledc_timer_config_t bl_timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 20000,           // 超過人耳上限，防止線圈聲
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t ret = ledc_timer_config(&bl_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC 計時器配置失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    const ledc_channel_config_t bl_channel = {
        .gpio_num   = CONFIG_GPIO_ILI9488_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0,
        .flags      = { .output_invert = 0 },
    };
    ret = ledc_channel_config(&bl_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC 通道配置失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

// 初始化 ILI9488 panel
static esp_err_t panel_init(void)
{
    // 步驟 1：建立 ILI9488 與 SPI2 溝通的 IO 橋樑
    const esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num         = CONFIG_GPIO_ILI9488_CS,
        .dc_gpio_num         = CONFIG_GPIO_ILI9488_DC,
        .spi_mode            = 0,
        .pclk_hz             = 60000000,    // 60 MHz，SPI2 IOMUX 腳位最高速
        .trans_queue_depth   = 10,          // 320 / 32 = 10，剛好容納一幀
        .on_color_trans_done = NULL,
        .user_ctx            = NULL,
        .lcd_cmd_bits        = 8,
        .lcd_param_bits      = 8,
    };
    esp_err_t ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &s_io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD IO 建立失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    // 步驟 2：ILI9488 面板配置
    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = CONFIG_GPIO_ILI9488_RESET,
        .color_space    = COLOR_RGB_ELEMENT_ORDER_BGR, // ILI9488 原生 BGR 排列
        .bits_per_pixel = 18, // RGB666
        .flags          = { .reset_active_high = 0 }, // RESET 拉低觸發
    };
    ret = esp_lcd_new_panel_ili9488(s_io_handle, &panel_cfg, CONFIG_ILI9488_WIDTH * 32, &s_panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "面板建立失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    // 步驟 3：重置
    ret = esp_lcd_panel_reset(s_panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "面板重置失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    // 步驟 4：發送初始化命令序列
    ret = esp_lcd_panel_init(s_panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "面板初始化失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    // 步驟 5：設定顯示方向與色彩（橫向 480×320）
    ret = esp_lcd_panel_invert_color(s_panel_handle, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "色彩反轉設定失敗: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_lcd_panel_swap_xy(s_panel_handle, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "XY 軸交換失敗: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_lcd_panel_mirror(s_panel_handle, true, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "鏡像設定失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    // 步驟 6：開啟顯示輸出
    ret = esp_lcd_panel_disp_on_off(s_panel_handle, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "顯示開啟失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

// ======================================================================
// 公開 API
// ======================================================================
// 初始化 ILI9488 顯示器與 LEDC PWM 背光
esp_err_t module_display_init(void)
{
    ESP_LOGI(TAG, "顯示器正在初始化");

    esp_err_t ret = backlight_init();
    if (ret != ESP_OK) return ret;

    ret = panel_init();
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "顯示器初始化完成");
    return ESP_OK;
}

// 設定背光亮度
void module_display_backlight_set(uint8_t percent)
{
    if (percent > 100) percent = 100;
    uint32_t duty = (uint32_t)percent * 255 / 100;  // 8-bit 解析度：0–255
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

// 取得 LCD panel IO handle 供 module_lvgl 使用
esp_lcd_panel_io_handle_t module_display_io_handle(void)
{
    return s_io_handle;
}

// 取得 LCD panel handle 供 module_lvgl 使用
esp_lcd_panel_handle_t module_display_panel_handle(void)
{
    return s_panel_handle;
}
