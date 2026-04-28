#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

// ======================================================================
// 型別定義
// ======================================================================

#define ALARM_MAX_COUNT  10

/**
 * @brief 鬧鐘資料
 *
 * repeat_days 為 bitmask，bit N 對應 weekday N（DS3231 慣例：1=週日…7=週六）。
 * repeat_days == 0 表示一次性鬧鐘，觸發後自動 disable。
 */
typedef struct {
    uint8_t hour;         // 0-23
    uint8_t minute;       // 0-59
    uint8_t repeat_days;  // bitmask，bit 1=週日…bit 7=週六，0=一次性
    uint8_t enabled;      // 1=啟用，0=停用
} alarm_t;

/** @brief 將 weekday（1-7）轉為 repeat_days bitmask 的位元遮罩 */
#define ALARM_WEEKDAY_BIT(wd)  ((uint8_t)(1u << (wd)))

/** @brief 鬧鐘觸發 callback，在 alarm_task context 中呼叫 */
typedef void (*alarm_trigger_cb_t)(int slot, const alarm_t *alarm);

// ======================================================================
// 公開 API
// ======================================================================

/**
 * @brief 初始化鬧鐘模組
 *
 * 從 NVS 載入所有鬧鐘資料，並啟動背景觸發任務。
 * 必須在 module_nvs_init() 和 module_time_init() 之後呼叫。
 *
 * @return ESP_OK 成功
 */
esp_err_t module_alarm_init(void);

/**
 * @brief 取得已設定的鬧鐘總數
 */
int module_alarm_count(void);

/**
 * @brief 讀取指定 slot 的鬧鐘資料
 *
 * @param slot slot index（0 ~ ALARM_MAX_COUNT-1）
 * @param[out] out 讀取結果
 * @return true slot 有效；false slot 為空
 */
bool module_alarm_get(int slot, alarm_t *out);

/**
 * @brief 新增一個鬧鐘，寫入第一個空 slot
 *
 * @param alarm 要新增的鬧鐘資料
 * @return slot index（0-9），已滿時回傳 -1
 */
int module_alarm_add(const alarm_t *alarm);

/**
 * @brief 覆寫指定 slot 的鬧鐘資料
 *
 * @param slot slot index
 * @param alarm 新的鬧鐘資料
 * @return ESP_OK 成功；ESP_ERR_INVALID_ARG slot 超出範圍
 */
esp_err_t module_alarm_set(int slot, const alarm_t *alarm);

/**
 * @brief 刪除指定 slot 的鬧鐘
 *
 * @param slot slot index
 * @return ESP_OK 成功
 */
esp_err_t module_alarm_delete(int slot);

/**
 * @brief 設定指定 slot 的啟用狀態
 *
 * @param slot    slot index
 * @param enabled true=啟用，false=停用
 * @return ESP_OK 成功
 */
esp_err_t module_alarm_set_enabled(int slot, bool enabled);

/**
 * @brief 取得下一個即將觸發的鬧鐘（供主畫面顯示）
 *
 * @param[out] out      鬧鐘資料
 * @param[out] slot_out slot index（可為 NULL）
 * @return true 有啟用中的鬧鐘；false 無
 */
bool module_alarm_get_next(alarm_t *out, int *slot_out);

/**
 * @brief 新增觸發 callback（最多 ALARM_MAX_COUNT 個）
 *
 * @param cb 鬧鐘觸發時呼叫的函式
 */
void module_alarm_add_trigger_cb(alarm_trigger_cb_t cb);

/**
 * @brief 移除觸發 callback
 *
 * @param cb 要移除的函式
 */
void module_alarm_remove_trigger_cb(alarm_trigger_cb_t cb);
