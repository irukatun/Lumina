// ESP-IDF 系統組件
#include <string.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <lwip/ip4_addr.h>

// 本專案模組
#include "module_wifi.h"

static const char *TAG = "M_WiFi";

// ======================================================================
// 私有常數
// ======================================================================

#define MAX_CALLBACKS   8
#define MAX_RETRY       5

// ======================================================================
// 私有變數
// ======================================================================

static module_wifi_event_cb_t s_callbacks[MAX_CALLBACKS];
static int                    s_callback_count = 0;
static bool                   s_connected      = false;
static char                   s_ip_str[16]     = "";
static int                    s_retry_count    = 0;
static bool                   s_started        = false;

// ======================================================================
// 私有函式
// ======================================================================

static void fire_callbacks(module_wifi_event_t event)
{
    for (int i = 0; i < s_callback_count; i++) {
        if (s_callbacks[i]) s_callbacks[i](event);
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *data)
{
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected   = false;
        s_ip_str[0]   = '\0';
        fire_callbacks(WIFI_EVT_DISCONNECTED);

        if (s_started && s_retry_count < MAX_RETRY) {
            s_retry_count++;
            ESP_LOGI(TAG, "重試連線 (%d/%d)", s_retry_count, MAX_RETRY);
            esp_wifi_connect();
        } else if (s_retry_count >= MAX_RETRY) {
            ESP_LOGW(TAG, "連線失敗，等待 provisioning（尚未實作）");
            // TODO: module_prov_start()
        }
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)data;
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&evt->ip_info.ip));
        s_connected   = true;
        s_retry_count = 0;
        ESP_LOGI(TAG, "已取得 IP：%s", s_ip_str);
        fire_callbacks(WIFI_EVT_CONNECTED);
    }
}

// ======================================================================
// 公開 API
// ======================================================================

esp_err_t module_wifi_init(void)
{
    ESP_LOGI(TAG, "初始化 WiFi");

    ESP_ERROR_CHECK(esp_netif_init());

    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

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

    ESP_LOGI(TAG, "WiFi 初始化完成");
    return ESP_OK;
}

void module_wifi_start(void)
{
    wifi_config_t cfg = {};
    esp_wifi_get_config(WIFI_IF_STA, &cfg);

    if (cfg.sta.ssid[0] != '\0') {
        ESP_LOGI(TAG, "找到已儲存的 SSID：%s，嘗試連線", (char *)cfg.sta.ssid);
        s_started     = true;
        s_retry_count = 0;
        esp_wifi_connect();
    } else {
        ESP_LOGW(TAG, "無已儲存的 WiFi credentials，等待 provisioning（尚未實作）");
        // TODO: module_prov_start()
    }
}

void module_wifi_stop(void)
{
    s_started = false;
    esp_wifi_disconnect();
}

bool module_wifi_is_connected(void)
{
    return s_connected;
}

void module_wifi_get_ip(char *buf, size_t len)
{
    strncpy(buf, s_ip_str, len - 1);
    buf[len - 1] = '\0';
}

void module_wifi_add_callback(module_wifi_event_cb_t cb)
{
    if (s_callback_count < MAX_CALLBACKS) {
        s_callbacks[s_callback_count++] = cb;
    }
}

void module_wifi_remove_callback(module_wifi_event_cb_t cb)
{
    for (int i = 0; i < s_callback_count; i++) {
        if (s_callbacks[i] == cb) {
            s_callbacks[i] = s_callbacks[--s_callback_count];
            s_callbacks[s_callback_count] = NULL;
            return;
        }
    }
}
