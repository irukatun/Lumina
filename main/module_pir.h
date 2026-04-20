#pragma once

// ESP-IDF 系統組件
#include <esp_err.h>
#include <stdbool.h>

typedef enum {
    PIR_EVENT_DETECTED,
    PIR_EVENT_CLEARED,
} pir_event_t;

typedef void (*pir_event_cb_t)(pir_event_t event);

/**
 * @brief 初始化 PIR 模組並啟動中斷監聽
 *
 * @return ESP_OK 成功；其他值表示初始化過程存在致命錯誤，呼叫端應重啟
 */
esp_err_t module_pir_init(void);

/**
 * @brief 讀取 PIR 當前狀態
 *
 * @return true 偵測到人體；false 無人
 */
bool module_pir_is_detected(void);

/**
 * @brief 新增 PIR 事件 callback（最多 4 個）
 *
 * @param cb 事件發生時呼叫的函式
 */
void module_pir_add_callback(pir_event_cb_t cb);

/**
 * @brief 移除 PIR 事件 callback
 *
 * @param cb 要移除的函式
 */
void module_pir_remove_callback(pir_event_cb_t cb);
