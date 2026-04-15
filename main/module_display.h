#pragma once

// ESP-IDF 系統組件
#include <esp_err.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

/**
 * @brief 初始化 ILI9488 顯示器與 LEDC PWM 背光
 *
 * @return ESP_OK 成功；其他值表示初始化過程存在致命錯誤，呼叫端應重啟
 */
esp_err_t module_display_init(void);

/**
 * @brief 設定背光亮度
 *
 * @param percent 亮度百分比 0–100
 */
void module_display_backlight_set(uint8_t percent);

/**
 * @brief 取得 LCD panel IO handle
 *
 * @return esp_lcd_panel_io_handle_t
 */
esp_lcd_panel_io_handle_t module_display_io_handle(void);

/**
 * @brief 取得 LCD panel handle
 *
 * @return esp_lcd_panel_handle_t
 */
esp_lcd_panel_handle_t module_display_panel_handle(void);
