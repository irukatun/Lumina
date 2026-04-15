#pragma once

// ESP-IDF 系統組件
#include <esp_err.h>

// 第三方組件
#include <esp_lcd_touch.h>

/**
 * @brief 初始化 XPT2046 觸控模組
 *
 * @return ESP_OK 成功；其他值表示初始化過程存在致命錯誤，呼叫端應重啟
 */
esp_err_t module_touch_init(void);

/**
 * @brief 取得觸控 handle
 *
 * @return esp_lcd_touch_handle_t
 */
esp_lcd_touch_handle_t module_touch_handle(void);
