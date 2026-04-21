#pragma once

#include <esp_err.h>

/**
 * @brief 執行完整啟動流程
 *
 *@return ESP_OK 成功；其他值表示啟動過程存在致命錯誤，呼叫端應重啟
 */
esp_err_t module_boot_run(void);