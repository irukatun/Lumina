#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <esp_err.h>

typedef enum {
    WIFI_EVT_CONNECTED,      // 已取得 IP，可以使用網路
    WIFI_EVT_DISCONNECTED,   // 連線中斷
    WIFI_EVT_CONNECT_FAILED, // 重試耗盡後仍無法連線
} module_wifi_event_t;

typedef void (*module_wifi_event_cb_t)(module_wifi_event_t event);

/**
 * @brief 初始化 WiFi 驅動（在 boot 階段呼叫，不發起連線）
 */
esp_err_t module_wifi_init(void);

/**
 * @brief 啟動連線（在主畫面顯示後呼叫）
 *
 * 若 NVS 有已儲存的 credentials 則直接連線；否則啟動 BLE provisioning。
 */
void module_wifi_start(void);

/**
 * @brief 斷開連線
 */
void module_wifi_stop(void);

/**
 * @brief 查詢目前是否已連線並取得 IP
 */
bool module_wifi_is_connected(void);

/**
 * @brief 取得目前 IP 字串（未連線時回傳空字串）
 */
void module_wifi_get_ip(char *buf, size_t len);

/**
 * @brief 新增 WiFi 事件 callback（最多 10 個）
 *
 * @param cb 事件發生時呼叫的函式
 */
void module_wifi_add_callback(module_wifi_event_cb_t cb);

/**
 * @brief 移除 WiFi 事件 callback
 *
 * @param cb 要移除的函式
 */
void module_wifi_remove_callback(module_wifi_event_cb_t cb);
