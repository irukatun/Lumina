// ESP-IDF 系統組件
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <lwip/netdb.h>

// 本專案模組
#include "module_network.h"
#include "module_wifi.h"

// 日誌標籤
static const char *TAG = "M_Net";

// ======================================================================
// 私有巨集
// ======================================================================

#define CHECK_HOST       "www.google.com"
#define CHECK_TASK_STACK 4096
#define MAX_CALLBACKS    10

// ======================================================================
// 私有變數
// ======================================================================

static module_network_cb_t      s_callbacks[MAX_CALLBACKS];
static module_network_status_t  s_status = NET_STATUS_UNAVAILABLE;

// ======================================================================
// 私有函式前向宣告
// ======================================================================

static void fire_callbacks(module_network_status_t status);
static void set_status(module_network_status_t new_status);
static void internet_check_task(void *arg);
static void wifi_cb(module_wifi_event_t event);

// ======================================================================
// 私有函式實作
// ======================================================================

static void fire_callbacks(module_network_status_t status)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (s_callbacks[i]) s_callbacks[i](status);
    }
}

static void set_status(module_network_status_t new_status)
{
    if (s_status == new_status) return;
    s_status = new_status;
    fire_callbacks(new_status);
}

static void internet_check_task(void *arg)
{
    struct addrinfo hints = {
        .ai_family   = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *result = NULL;

    int err = getaddrinfo(CHECK_HOST, NULL, &hints, &result);
    if (err == 0 && result != NULL) {
        freeaddrinfo(result);
        ESP_LOGI(TAG, "網際網路可用");
        set_status(NET_STATUS_AVAILABLE);
    } else {
        ESP_LOGW(TAG, "DNS 解析失敗，網路不可用（err=%d）", err);
        set_status(NET_STATUS_UNAVAILABLE);
    }

    vTaskDelete(NULL);
}

static void wifi_cb(module_wifi_event_t event)
{
    if (event == WIFI_EVT_CONNECTED) {
        ESP_LOGI(TAG, "WiFi 已連線，開始檢測網際網路");
        xTaskCreate(internet_check_task, "net_check", CHECK_TASK_STACK,
                    NULL, 5, NULL);
    } else if (event == WIFI_EVT_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi 已斷線");
        set_status(NET_STATUS_UNAVAILABLE);
    }
}

// ======================================================================
// 公開 API
// ======================================================================

esp_err_t module_network_init(void)
{
    module_wifi_add_callback(wifi_cb);
    ESP_LOGI(TAG, "初始化完成");
    return ESP_OK;
}

module_network_status_t module_network_get_status(void)
{
    return s_status;
}

void module_network_add_callback(module_network_cb_t cb)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (!s_callbacks[i]) {
            s_callbacks[i] = cb;
            return;
        }
    }
    ESP_LOGW(TAG, "callback 已滿");
}

void module_network_remove_callback(module_network_cb_t cb)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (s_callbacks[i] == cb) {
            s_callbacks[i] = NULL;
            return;
        }
    }
}
