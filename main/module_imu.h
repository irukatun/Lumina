#pragma once

// ESP-IDF 系統組件
#include <esp_err.h>

typedef enum {
    MODULE_IMU_STATUS_OK,
    MODULE_IMU_STATUS_ERROR,
} module_imu_status_t;

typedef enum {
    IMU_EVENT_TAP,
    IMU_EVENT_SHAKE_START,
    IMU_EVENT_SHAKE_END,
} imu_event_t;

typedef void (*imu_event_cb_t)(imu_event_t event);

/**
 * @brief 初始化 MPU6050 IMU 模組並啟動感測任務
 *
 * @return ESP_OK 成功；其他值表示初始化過程存在致命錯誤，呼叫端應重啟
 */
esp_err_t module_imu_init(void);

/**
 * @brief 查詢 IMU 初始化狀態
 *
 * @return module_imu_status_t
 */
module_imu_status_t module_imu_get_status(void);

/**
 * @brief 新增 IMU 事件 callback（最多 10 個）
 *
 * @param cb 事件發生時呼叫的函式
 */
void module_imu_add_callback(imu_event_cb_t cb);

/**
 * @brief 移除 IMU 事件 callback
 *
 * @param cb 要移除的函式
 */
void module_imu_remove_callback(imu_event_cb_t cb);
