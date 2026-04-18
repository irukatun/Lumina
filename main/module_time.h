#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief RTC 狀態
 */
typedef enum {
    MODULE_TIME_STATUS_OK,          // 時間可信
    MODULE_TIME_STATUS_POWER_LOSS,  // 電源中斷（OSF），時間不可信
    MODULE_TIME_STATUS_RTC_ERROR,   // 硬體故障或未接
} module_time_status_t;

/**
 * @brief 時間資料
 */
typedef struct {
    int year;    // 西元年，例如 2026
    int month;   // 月份 1-12
    int day;     // 日 1-31
    int hour;    // 時 0-23
    int minute;  // 分 0-59
    int second;  // 秒 0-59
    int weekday; // 星期 1–7，對應關係由使用者校時時自行定義
} module_time_t;

/**
 * @brief 初始化 DS3231 RTC 模組
 *
 * 必須在 module_bus_init() 之後呼叫。
 *
 * @return ESP_OK 成功；其他值表示初始化失敗
 */
esp_err_t module_time_init(void);

/**
 * @brief 讀取 DS3231 當前時間
 *
 * @param[out] out 讀取結果
 * @return ESP_OK 成功
 */
esp_err_t module_time_get(module_time_t *out);

/**
 * @brief 將時間寫入 DS3231
 *
 * @param[in] t 要寫入的時間
 * @return ESP_OK 成功
 */
esp_err_t module_time_set(const module_time_t *t);

/**
 * @brief 查詢 RTC 目前狀態
 *
 * @return module_time_status_t
 */
module_time_status_t module_time_get_status(void);

/**
 * @brief 讀取 DS3231 內建溫度感測器
 *
 * 精度 ±3°C，每 64 秒更新一次，反映環境溫度。
 *
 * @param[out] out 溫度（°C）
 * @return ESP_OK 成功
 */
esp_err_t module_time_get_temp(float *out);
