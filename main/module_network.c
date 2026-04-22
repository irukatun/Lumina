// C 標準函式庫
#include <string.h>

// ESP-IDF 系統組件
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <lwip/ip4_addr.h>
#include <lwip/netdb.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>

// 本專案模組
#include "module_network.h"

// 日誌標籤
static const char *TAG = "M_Net";

// ======================================================================
// 私有巨集
// ======================================================================

#define DEVICE_NAME      "小鹿米智慧桌上助理"
#define POP              "lumina"
#define MAX_RETRY        5
#define MAX_CALLBACKS    10
#define CHECK_HOST            "www.google.com"
#define CHECK_TASK_STACK      8192
#define NET_CHECK_INTERVAL_MS (60 * 1000)

// ======================================================================
// 私有變數
// ======================================================================

// WiFi 狀態
static char s_ip_str[16]  = "";
static int  s_retry_count = 0;
static bool s_started     = false;

// 配網資源釋放旗標
static bool s_prov_released = false;
static bool s_prov_success  = false;

// 網路檢查任務 handle（用於喚醒立刻檢查）
static TaskHandle_t s_check_task = NULL;

// 網路狀態
static module_network_status_t s_status = NET_STATUS_NOWIFI;

// Callback 陣列
static module_network_event_cb_t s_event_cbs[MAX_CALLBACKS];
static module_network_prov_cb_t  s_prov_cbs[MAX_CALLBACKS];

// ======================================================================
// 私有函式前向宣告
// ======================================================================

static void fire_event_callbacks(module_network_event_t event);
static void fire_prov_callbacks(module_network_prov_event_t event);
static void internet_check_task(void *arg);
static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data);
static void prov_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data);

// ======================================================================
// 私有函式實作
// ======================================================================

static void fire_event_callbacks(module_network_event_t event)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (s_event_cbs[i]) s_event_cbs[i](event);
    }
}

static void fire_prov_callbacks(module_network_prov_event_t event)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (s_prov_cbs[i]) s_prov_cbs[i](event);
    }
}

static void internet_check_task(void *arg)
{
    while (1) {
        xTaskNotifyWait(0, 0, NULL, pdMS_TO_TICKS(NET_CHECK_INTERVAL_MS));

        if (s_status == NET_STATUS_NOWIFI) continue;

        struct addrinfo hints = {
            .ai_family   = AF_INET,
            .ai_socktype = SOCK_STREAM,
        };
        struct addrinfo *result = NULL;

        int err = getaddrinfo(CHECK_HOST, NULL, &hints, &result);
        if (err == 0 && result != NULL) {
            freeaddrinfo(result);
            if (s_status != NET_STATUS_OK) {
                ESP_LOGI(TAG, "網際網路可用");
                s_status = NET_STATUS_OK;
                fire_event_callbacks(NET_EVT_INTERNET_UP);
            }
        } else {
            ESP_LOGW(TAG, "DNS 解析失敗（err=%d）", err);
            if (s_status != NET_STATUS_NONET) {
                s_status = NET_STATUS_NONET;
                fire_event_callbacks(NET_EVT_INTERNET_DOWN);
            }
        }
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_ip_str[0] = '\0';
        s_status    = NET_STATUS_NOWIFI;
        fire_event_callbacks(NET_EVT_DISCONNECTED);

        if (s_started && s_retry_count < MAX_RETRY) {
            s_retry_count++;
            ESP_LOGI(TAG, "重試連線 (%d/%d)", s_retry_count, MAX_RETRY);
            esp_wifi_connect();
        } else if (s_retry_count >= MAX_RETRY) {
            ESP_LOGW(TAG, "重試耗盡，連線失敗");
            s_started = false;
            fire_event_callbacks(NET_EVT_CONNECT_FAILED);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)data;
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&evt->ip_info.ip));
        s_status      = NET_STATUS_NONET;
        s_started     = true;
        s_retry_count = 0;
        ESP_LOGI(TAG, "已取得 IP：%s", s_ip_str);
        fire_event_callbacks(NET_EVT_CONNECTED);
        if (s_check_task) xTaskNotify(s_check_task, 0, eNoAction);
    }
}

static void prov_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    switch (id) {
        case WIFI_PROV_START:
            ESP_LOGI(TAG, "配網開始，裝置名稱：%s", DEVICE_NAME);
            fire_prov_callbacks(NET_PROV_EVT_STARTED);
            break;

        case WIFI_PROV_CRED_RECV: {
            wifi_sta_config_t *cfg = (wifi_sta_config_t *)data;
            ESP_LOGI(TAG, "收到憑證，SSID：%s", (char *)cfg->ssid);
            fire_prov_callbacks(NET_PROV_EVT_CRED_RECV);
            break;
        }

        case WIFI_PROV_CRED_FAIL: {
            wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)data;
            ESP_LOGE(TAG, "憑證錯誤：%s",
                     *reason == WIFI_PROV_STA_AUTH_ERROR ? "密碼錯誤" : "找不到 AP");
            fire_prov_callbacks(NET_PROV_EVT_CRED_FAIL);
            break;
        }

        case WIFI_PROV_CRED_SUCCESS:
            ESP_LOGI(TAG, "憑證驗證成功，WiFi 已連線");
            s_prov_success = true;
            fire_prov_callbacks(NET_PROV_EVT_DONE);
            break;

        case WIFI_PROV_END:
            ESP_LOGI(TAG, "配網服務結束");
            if (!s_prov_released) {
                s_prov_released = true;
                wifi_prov_mgr_deinit();
                ESP_LOGI(TAG, "prov 資源已釋放");
            }
            break;

        default:
            break;
    }
}

// ======================================================================
// 公開 API
// ======================================================================

esp_err_t module_network_init(void)
{
    ESP_LOGI(TAG, "初始化 WiFi 驅動");

    ESP_ERROR_CHECK(esp_netif_init());

    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) return ret;

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_prov_mgr_config_t prov_cfg = {
        .scheme               = wifi_prov_scheme_ble,
        // 配網完成後自動釋放 BLE 堆疊，節省 SRAM
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BLE,
        // 非密碼錯誤的斷線（如 AP 踢掉）最多重試 3 次再觸發 CRED_FAIL
        .wifi_prov_conn_cfg   = { .wifi_conn_attempts = 3 },
    };

    ret = wifi_prov_mgr_init(prov_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "prov init 失敗：%s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID,
                                     prov_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "prov event handler 註冊失敗：%s", esp_err_to_name(ret));
        return ret;
    }

    s_prov_released = false;
    s_prov_success  = false;
    xTaskCreate(internet_check_task, "net_check", CHECK_TASK_STACK, NULL, 5, &s_check_task);
    ESP_LOGI(TAG, "初始化完成");
    return ESP_OK;
}

module_network_status_t module_network_get_status(void)
{
    return s_status;
}

int8_t module_network_get_rssi(void)
{
    if (s_status == NET_STATUS_NOWIFI) return 0;
    int rssi = 0;
    esp_wifi_sta_get_rssi(&rssi);
    return (int8_t)rssi;
}

void module_network_get_ip(char *buf, size_t len)
{
    strncpy(buf, s_ip_str, len - 1);
    buf[len - 1] = '\0';
}

bool module_network_is_provisioned(void)
{
    bool provisioned = false;
    wifi_prov_mgr_is_provisioned(&provisioned);
    return provisioned;
}

void module_network_connect(void)
{
    if (s_status != NET_STATUS_NOWIFI) {
        ESP_LOGI(TAG, "已連線，跳過");
        return;
    }
    ESP_LOGI(TAG, "開始連線");
    s_started     = true;
    s_retry_count = 0;
    esp_wifi_connect();
}

void module_network_disconnect(void)
{
    s_started = false;
    esp_wifi_disconnect();
}

void module_network_provision_start(void)
{
    ESP_LOGI(TAG, "開始配網（BLE 廣播中），free heap: %lu", esp_get_free_heap_size());
    wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, POP, DEVICE_NAME, NULL);
}

void module_network_provision_stop(void)
{
    wifi_prov_mgr_stop_provisioning();
}

void module_network_release_prov(void)
{
    if (s_prov_released) return;
    if (s_prov_success) return;  // 配網成功：等 auto-stop 觸發 WIFI_PROV_END 再 deinit
    s_prov_released = true;
    wifi_prov_mgr_deinit();
    ESP_LOGI(TAG, "prov 資源已釋放");
}

void module_network_add_event_callback(module_network_event_cb_t cb)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (!s_event_cbs[i]) {
            s_event_cbs[i] = cb;
            return;
        }
    }
    ESP_LOGW(TAG, "event callback 已滿");
}

void module_network_remove_event_callback(module_network_event_cb_t cb)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (s_event_cbs[i] == cb) {
            s_event_cbs[i] = NULL;
            return;
        }
    }
}

void module_network_add_prov_callback(module_network_prov_cb_t cb)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (!s_prov_cbs[i]) {
            s_prov_cbs[i] = cb;
            return;
        }
    }
    ESP_LOGW(TAG, "prov callback 已滿");
}

void module_network_remove_prov_callback(module_network_prov_cb_t cb)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (s_prov_cbs[i] == cb) {
            s_prov_cbs[i] = NULL;
            return;
        }
    }
}
