#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <time.h>

// ======================================================================
// 公開型別
// ======================================================================

/**
 * @brief 同步狀態
 *
 * 反映定期同步任務的進度，供 UI 決定是否顯示同步動畫。
 */
typedef enum {
    SYNC_STATUS_IDLE,     // 尚未執行過同步
    SYNC_STATUS_SYNCING,  // 同步進行中
    SYNC_STATUS_DONE,     // 上次同步成功
    SYNC_STATUS_FAILED,   // 上次同步失敗（含 NTP 逾時）
} module_sync_status_t;

/**
 * @brief 天氣資料
 */
typedef struct {
    float    outdoor_temp;      // 室外氣溫（°C）
    char     city[32];          // 城市顯示名（由 Geo API 回傳）
    char     weather_text[32];  // 天氣描述（如「晴」）
    int      humidity;          // 相對濕度（%）
    time_t   updated_at;        // 最後成功更新的 Unix timestamp
} module_sync_weather_t;

typedef void (*module_sync_cb_t)(module_sync_status_t status);

// ======================================================================
// 公開 API
// ======================================================================

/**
 * @brief 初始化同步模組
 *
 * 啟動長駐定期任務（立即執行第一次，之後每 30 分鐘執行一次）。
 * 非阻塞，任務內部自行判斷網路是否可用。
 *
 * @return ESP_OK 成功
 */
esp_err_t module_sync_init(void);

/**
 * @brief 查詢目前同步狀態
 *
 * @return module_sync_status_t
 */
module_sync_status_t module_sync_get_status(void);

/**
 * @brief 取得天氣資料
 *
 * @param[out] out 天氣資料
 * @return true 有資料；false 尚未同步
 */
bool module_sync_get_weather(module_sync_weather_t *out);

/**
 * @brief 新增同步狀態變化 callback（最多 10 個）
 *
 * @param cb 狀態變化時呼叫的函式
 */
void module_sync_add_callback(module_sync_cb_t cb);

/**
 * @brief 移除同步狀態變化 callback
 *
 * @param cb 要移除的函式
 */
void module_sync_remove_callback(module_sync_cb_t cb);
