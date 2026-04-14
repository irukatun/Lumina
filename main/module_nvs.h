#pragma once

#include <esp_err.h>
#include <stdbool.h>

/**
 * @brief NVS 初始化
 *
 * @return ESP_OK 成功；其他值表示初始化過程存在致命錯誤，呼叫端應重啟
 */
esp_err_t module_nvs_init(void);

/**
 * @brief 查詢本次開機是否偵測到韌體版本更新
 *
 * @return true 偵測到版本更新；false 版本未變更
 */
bool module_nvs_ver_updated(void);

/**
 * @brief 查詢 NVS 是否因損毀而被重置
 *
 * @return true 表示 NVS 被重置；false 表示 NVS 正常
 */
bool module_nvs_reset(void);
