#pragma once

// ESP-IDF 系統組件
#include <esp_err.h>
#include <driver/i2c_master.h>

/**
 * @brief 初始化所有硬體匯流排
 * @return ESP_OK 成功；其他值表示匯流排啟動過程存在致命錯誤，呼叫端應重啟
 */
esp_err_t module_bus_init(void);

/**
 * @brief 取得 I2C0 匯流排 handle
 * @return i2c_master_bus_handle_t
 */
i2c_master_bus_handle_t module_bus_i2c0_handle(void);
