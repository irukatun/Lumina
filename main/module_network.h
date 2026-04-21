#pragma once

#include <esp_err.h>

// ======================================================================
// 公開型別
// ======================================================================

typedef enum {
    NET_STATUS_UNAVAILABLE,  // 無 WiFi 或有 WiFi 但無網際網路
    NET_STATUS_AVAILABLE,    // 網際網路可用
} module_network_status_t;

typedef void (*module_network_cb_t)(module_network_status_t status);

// ======================================================================
// 公開 API
// ======================================================================

/**
 * @brief 初始化網路狀態模組
 *
 * 訂閱 module_wifi 事件，並在 WiFi 連線後自動發起網際網路可用性檢測。
 *
 * @return ESP_OK 成功
 */
esp_err_t module_network_init(void);

/**
 * @brief 查詢目前網路狀態
 *
 * @return module_network_status_t
 */
module_network_status_t module_network_get_status(void);

/**
 * @brief 新增網路狀態變化 callback（最多 10 個）
 *
 * @param cb 狀態變化時呼叫的函式
 */
void module_network_add_callback(module_network_cb_t cb);

/**
 * @brief 移除網路狀態變化 callback
 *
 * @param cb 要移除的函式
 */
void module_network_remove_callback(module_network_cb_t cb);
