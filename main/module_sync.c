// C 標準函式庫
#include <string.h>
#include <time.h>

// ESP-IDF 系統組件
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_netif_sntp.h>

// 本專案模組
#include "module_sync.h"
#include "module_network.h"
#include "module_time.h"

// 日誌標籤
static const char *TAG = "M_Sync";

// ======================================================================
// 私有巨集
// ======================================================================

#define MAX_CALLBACKS    10
#define SYNC_TASK_STACK  4096
#define NTP_SERVER       "pool.ntp.org"
#define NTP_TIMEOUT_MS   30000
#define TIMEZONE         "CST-8"  // UTC+8，台灣標準時間
#define SYNC_INTERVAL_MS (30 * 60 * 1000)

// ======================================================================
// 私有變數
// ======================================================================

static module_sync_status_t  s_status      = SYNC_STATUS_IDLE;
static module_sync_cb_t      s_callbacks[MAX_CALLBACKS];
static module_sync_weather_t s_weather;
static bool                  s_has_weather = false;

// ======================================================================
// 私有函式前向宣告
// ======================================================================

static void fire_callbacks(module_sync_status_t status);
static void set_status(module_sync_status_t new_status);
static void sync_task(void *arg);

// ======================================================================
// 私有函式實作
// ======================================================================

static void fire_callbacks(module_sync_status_t status)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (s_callbacks[i]) s_callbacks[i](status);
    }
}

static void set_status(module_sync_status_t new_status)
{
    if (s_status == new_status) return;
    s_status = new_status;
    fire_callbacks(new_status);
}

static void sync_task(void *arg)
{
    while (1) {
        if (module_network_get_status() != NET_STATUS_OK) {
            ESP_LOGI(TAG, "網路不可用，跳過本次同步");
            vTaskDelay(pdMS_TO_TICKS(SYNC_INTERVAL_MS));
            continue;
        }

        ESP_LOGI(TAG, "開始同步");
        set_status(SYNC_STATUS_SYNCING);

        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER);
        esp_netif_sntp_init(&config);

        esp_err_t ret = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(NTP_TIMEOUT_MS));
        esp_netif_sntp_deinit();

        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "NTP 同步逾時");
            set_status(SYNC_STATUS_FAILED);
            vTaskDelay(pdMS_TO_TICKS(SYNC_INTERVAL_MS));
            continue;
        }

        setenv("TZ", TIMEZONE, 1);
        tzset();

        time_t now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);

        module_time_t rtc = {
            .year    = t.tm_year + 1900,
            .month   = t.tm_mon + 1,
            .day     = t.tm_mday,
            .hour    = t.tm_hour,
            .minute  = t.tm_min,
            .second  = t.tm_sec,
            // tm_wday: 0=日，轉換為 1(一)–7(日) ISO 8601
            .weekday = t.tm_wday == 0 ? 7 : t.tm_wday,
        };

        ret = module_time_set(&rtc);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "時間寫入 DS3231 失敗: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "時間同步完成: %04d-%02d-%02d %02d:%02d:%02d",
                     rtc.year, rtc.month, rtc.day,
                     rtc.hour, rtc.minute, rtc.second);
        }

        // TODO: 天氣 API 請求
        // s_weather.outdoor_temp = ...;
        // s_has_weather = true;

        set_status(SYNC_STATUS_DONE);
        vTaskDelay(pdMS_TO_TICKS(SYNC_INTERVAL_MS));
    }
}

// ======================================================================
// 公開 API
// ======================================================================

esp_err_t module_sync_init(void)
{
    memset(s_callbacks, 0, sizeof(s_callbacks));
    s_has_weather = false;
    xTaskCreate(sync_task, "sync_task", SYNC_TASK_STACK, NULL, 5, NULL);
    ESP_LOGI(TAG, "初始化完成");
    return ESP_OK;
}

module_sync_status_t module_sync_get_status(void)
{
    return s_status;
}

bool module_sync_get_weather(module_sync_weather_t *out)
{
    if (!s_has_weather) return false;
    *out = s_weather;
    return true;
}

void module_sync_add_callback(module_sync_cb_t cb)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (!s_callbacks[i]) {
            s_callbacks[i] = cb;
            return;
        }
    }
    ESP_LOGW(TAG, "callback 已滿");
}

void module_sync_remove_callback(module_sync_cb_t cb)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (s_callbacks[i] == cb) {
            s_callbacks[i] = NULL;
            return;
        }
    }
}
