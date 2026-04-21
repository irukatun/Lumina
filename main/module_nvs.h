#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 初始化 NVS 並進行韌體版本與更新
 *
 * @return ESP_OK 成功；其他值表示初始化過程存在致命錯誤，呼叫端應重啟
 */
esp_err_t module_nvs_init(void);

/**
 * @brief 讀取指定 namespace / key 的 u8 值
 *
 * @param ns  namespace
 * @param key key
 * @param[out] out 讀取結果
 * @return ESP_OK 成功；ESP_ERR_NVS_NOT_FOUND 表示 key 不存在
 */
esp_err_t module_nvs_get_u8(const char *ns, const char *key, uint8_t *out);

/**
 * @brief 寫入指定 namespace / key 的 u8 值（自動 commit）
 *
 * @param ns    namespace
 * @param key   key
 * @param value 寫入值
 * @return ESP_OK 成功
 */
esp_err_t module_nvs_set_u8(const char *ns, const char *key, uint8_t value);

/**
 * @brief 查詢本次開機是否偵測到韌體版本更新
 *
 * @return true 偵測到版本更新；false 版本未變更
 */
bool module_nvs_is_ver_updated(void);

/**
 * @brief 查詢 NVS 是否因損毀而被重置
 *
 * @return true 表示 NVS 被重置；false 表示 NVS 正常
 */
bool module_nvs_is_reset(void);
