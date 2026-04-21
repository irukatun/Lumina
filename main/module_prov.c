// ESP-IDF 系統組件
#include <esp_log.h>
#include <esp_err.h>
#include <esp_event.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>

// 本專案模組
#include "module_prov.h"

// 日誌標籤
static const char *TAG = "M_Prov";

// ======================================================================
// 私有巨集
// ======================================================================

#define DEVICE_NAME   "小鹿米智慧桌上助理"
#define POP           "lumina"
#define MAX_CALLBACKS 10

// ======================================================================
// 私有變數
// ======================================================================

static module_prov_event_cb_t s_callbacks[MAX_CALLBACKS];

// ======================================================================
// 私有函式前向宣告
// ======================================================================

static void fire_event(module_prov_event_t event);
static void prov_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data);

// ======================================================================
// 私有函式實作
// ======================================================================

static void fire_event(module_prov_event_t event)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (s_callbacks[i]) s_callbacks[i](event);
    }
}

static void prov_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    switch (id) {
        case WIFI_PROV_START:
            ESP_LOGI(TAG, "配網開始，裝置名稱：%s", DEVICE_NAME);
            fire_event(PROV_EVT_STARTED);
            break;

        case WIFI_PROV_CRED_RECV: {
            wifi_sta_config_t *cfg = (wifi_sta_config_t *)data;
            ESP_LOGI(TAG, "收到憑證，SSID：%s", (char *)cfg->ssid);
            fire_event(PROV_EVT_CRED_RECEIVED);
            break;
        }

        case WIFI_PROV_CRED_FAIL: {
            wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)data;
            ESP_LOGE(TAG, "憑證錯誤：%s",
                     *reason == WIFI_PROV_STA_AUTH_ERROR ? "密碼錯誤" : "找不到 AP");
            // ESP-IDF 限制：CRED_FAIL 後服務持續運行但不再接受新憑證，必須重啟
            fire_event(PROV_EVT_CRED_FAILED);
            break;
        }

        case WIFI_PROV_CRED_SUCCESS:
            ESP_LOGI(TAG, "憑證驗證成功，WiFi 已連線");
            fire_event(PROV_EVT_COMPLETED);
            break;

        case WIFI_PROV_END:
            ESP_LOGI(TAG, "配網服務結束，釋放資源");
            wifi_prov_mgr_deinit();
            break;

        default:
            break;
    }
}

// ======================================================================
// 公開 API
// ======================================================================

esp_err_t module_prov_init(void)
{
    wifi_prov_mgr_config_t config = {
        .scheme               = wifi_prov_scheme_ble,
        // 配網完成後自動釋放 BLE 堆疊，節省 SRAM
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BLE,
    };

    esp_err_t ret = wifi_prov_mgr_init(config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "init 失敗：%s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID,
                                     prov_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "event handler 註冊失敗：%s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "初始化完成");
    return ESP_OK;
}

bool module_prov_is_provisioned(void)
{
    bool provisioned = false;
    wifi_prov_mgr_is_provisioned(&provisioned);
    return provisioned;
}

esp_err_t module_prov_start(void)
{
    ESP_LOGI(TAG, "開始配網（BLE 廣播中）");
    return wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, POP,
                                            DEVICE_NAME, NULL);
}

void module_prov_stop(void)
{
    wifi_prov_mgr_stop_provisioning();
}

void module_prov_add_callback(module_prov_event_cb_t cb)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (!s_callbacks[i]) {
            s_callbacks[i] = cb;
            return;
        }
    }
    ESP_LOGW(TAG, "callback 已滿");
}

void module_prov_remove_callback(module_prov_event_cb_t cb)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (s_callbacks[i] == cb) {
            s_callbacks[i] = NULL;
            return;
        }
    }
}
