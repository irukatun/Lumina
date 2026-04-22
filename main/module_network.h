#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <esp_err.h>

// ======================================================================
// 公開型別
// ======================================================================

typedef enum {
    NET_STATUS_NOWIFI,  // 無 IP（WiFi 未連線）
    NET_STATUS_NONET,   // 有 IP 但 DNS 失敗（無網際網路）
    NET_STATUS_OK,      // 有 IP 且 DNS 通過
} module_network_status_t;

// 連線事件（WiFi 連線狀態 + DNS 驗證結果，供 module_ui_network 訂閱）
typedef enum {
    NET_EVT_CONNECTED,       // 取得 IP
    NET_EVT_DISCONNECTED,    // 斷線
    NET_EVT_CONNECT_FAILED,  // 重試耗盡
    NET_EVT_INTERNET_UP,     // DNS 驗證通過
    NET_EVT_INTERNET_DOWN,   // DNS 驗證失敗
} module_network_event_t;

typedef void (*module_network_event_cb_t)(module_network_event_t event);

// 配網事件（供 module_ui_network 訂閱）
typedef enum {
    NET_PROV_EVT_STARTED,    // BLE 廣播開始
    NET_PROV_EVT_CRED_RECV,  // 收到憑證，正在驗證
    NET_PROV_EVT_CRED_FAIL,  // 憑證錯誤
    NET_PROV_EVT_DONE,       // 配網成功，WiFi 已連線
} module_network_prov_event_t;

typedef void (*module_network_prov_cb_t)(module_network_prov_event_t event);

// ======================================================================
// 公開 API
// ======================================================================

// 第一進入點：boot 階段呼叫，初始化 WiFi 驅動與配網管理器（非阻塞）
esp_err_t module_network_init(void);

// 狀態查詢
module_network_status_t module_network_get_status(void);
void                    module_network_get_ip(char *buf, size_t len);
int8_t                  module_network_get_rssi(void);   // 未連線時回傳 0
bool                    module_network_is_provisioned(void);

// WiFi 操作（module_ui_network 呼叫）
void module_network_connect(void);
void module_network_disconnect(void);

// 配網操作（module_ui_network 呼叫）
void module_network_provision_start(void);
void module_network_provision_stop(void);
void module_network_release_prov(void);  // ui_network_show() return 前呼叫，無條件釋放 prov 資源

// Callback（僅供 module_ui_network 使用）
void module_network_add_event_callback(module_network_event_cb_t cb);
void module_network_remove_event_callback(module_network_event_cb_t cb);
void module_network_add_prov_callback(module_network_prov_cb_t cb);
void module_network_remove_prov_callback(module_network_prov_cb_t cb);
