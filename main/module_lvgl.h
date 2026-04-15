#pragma once

// ESP-IDF 系統組件
#include <esp_err.h>

/**
 * @brief 初始化 LVGL 並整合顯示器與觸控輸入
 *
 * 須在 module_display_init() 及 module_touch_init() 之後呼叫
 * 完成後其他模組可直接呼叫 lv_xxx API 進行繪製
 *
 * @return ESP_OK 成功；其他值表示初始化失敗
 */
esp_err_t module_lvgl_init(void);
