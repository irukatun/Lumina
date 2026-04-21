#pragma once

#include <stdbool.h>
#include <esp_err.h>

// ======================================================================
// 公開型別
// ======================================================================

typedef enum {
    PROV_EVT_STARTED,        // BLE 開始廣播，等待手機配對
    PROV_EVT_CRED_RECEIVED,  // 收到憑證，正在嘗試 WiFi 連線
    PROV_EVT_CRED_FAILED,    // WiFi 連線失敗（密碼錯誤或找不到 AP），需重啟
    PROV_EVT_COMPLETED,      // 配網完成，WiFi 已連線
} module_prov_event_t;

typedef void (*module_prov_event_cb_t)(module_prov_event_t event);

// ======================================================================
// 公開 API
// ======================================================================

/**
 * @brief 初始化 BLE provisioning manager
 *
 * 註冊事件 handler 並準備好 BLE 堆疊。不會主動開始廣播。
 *
 * @return ESP_OK 成功
 */
esp_err_t module_prov_init(void);

/**
 * @brief 查詢裝置是否已完成配網（NVS 中存有 credentials）
 *
 * @return true 已配網；false 尚未配網
 */
bool module_prov_is_provisioned(void);

/**
 * @brief 開始配網（啟動 BLE 廣播並等待手機連線）
 *
 * @return ESP_OK 成功；其他值表示啟動失敗
 */
esp_err_t module_prov_start(void);

/**
 * @brief 停止配網流程
 */
void module_prov_stop(void);

/**
 * @brief 新增配網事件 callback（最多 10 個）
 *
 * @param cb 事件發生時呼叫的函式
 */
void module_prov_add_callback(module_prov_event_cb_t cb);

/**
 * @brief 移除配網事件 callback
 *
 * @param cb 要移除的函式
 */
void module_prov_remove_callback(module_prov_event_cb_t cb);
