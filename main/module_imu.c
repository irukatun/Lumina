// C 標準函式庫
#include <math.h>

// ESP-IDF 系統組件
#include <esp_log.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// 第三方組件
#include <mpu6050.h>

// 本專案模組
#include "board.h"
#include "module_bus.h"
#include "module_imu.h"

// 日誌標籤
static const char *TAG = "M_IMU";

// ======================================================================
// 私有巨集
// ======================================================================
#define IMU_TASK_STACK_SIZE     4096
#define IMU_TASK_PRIORITY       5
#define IMU_POLL_INTERVAL_MS    20          // 50Hz 輪詢

#define IMU_BASE_X              0.0274f     // 實測靜止基線
#define IMU_BASE_Y              0.008f      // 實測靜止基線
#define IMU_BASE_Z              1.97f       // 實測靜止基線

#define TAP_ACCEL_THRESHOLD     0.7f        // g，超過視為敲擊尖峰（桌面雜訊實測最高 0.6g）
#define TAP_MAX_CYCLES          3           // 尖峰超過此週期數視為非敲擊

#define SHAKE_ENTRY_THRESHOLD       0.15f   // g，超過此值進入偵測狀態
#define SHAKE_LOW_THRESHOLD         0.07f   // g，低於此值視為靜止樣本
#define SHAKE_DETECT_WINDOW         25      // 偵測視窗樣本數
#define SHAKE_MAX_QUIET_IN_WINDOW   10      // 視窗內靜止樣本上限（少於此值確認搖晃）
#define SHAKE_EXIT_CONSECUTIVE      25      // 連續靜止樣本數才退出搖晃

// ======================================================================
// 私有變數
// ======================================================================
static mpu6050_handle_t s_mpu6050_handle        = NULL;
static imu_event_cb_t   s_event_cbs[4]          = {NULL};
static TaskHandle_t     s_imu_task_handle        = NULL;

// ======================================================================
// 私有函式前向宣告
// ======================================================================
static esp_err_t mpu6050_device_init(void);
static int       detect_tap(const mpu6050_accel_data_axes_t *accel);
static int       detect_shake(const mpu6050_accel_data_axes_t *accel);
static void      imu_task(void *arg);

// ======================================================================
// 私有函式實作
// ======================================================================
// 初始化 MPU6050 裝置並掛載至 I2C0 匯流排
static esp_err_t mpu6050_device_init(void)
{
    const mpu6050_config_t config = {
        .i2c_address            = CONFIG_MPU6050_ADDR,
        .i2c_clock_speed        = I2C_MPU6050_DEV_CLK_SPD,
        .low_pass_filter        = MPU6050_DIGITAL_LP_FILTER_ACCEL_260KHZ_GYRO_256KHZ,
        .gyro_clock_source      = MPU6050_GYRO_CS_PLL_X_AXIS_REF,
        .gyro_full_scale_range  = MPU6050_GYRO_FS_RANGE_500DPS,
        .accel_full_scale_range = MPU6050_ACCEL_FS_RANGE_4G,
    };

    esp_err_t ret = mpu6050_init(module_bus_i2c0_handle(), &config, &s_mpu6050_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 初始化失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

// 敲擊偵測：任意方向短暫加速度尖峰
static int detect_tap(const mpu6050_accel_data_axes_t *accel)
{
    static uint32_t s_spike_cycles = 0;
    static bool     s_in_spike     = false;

    float dx = accel->x_axis - IMU_BASE_X;
    float dy = accel->y_axis - IMU_BASE_Y;
    float dz = accel->z_axis - IMU_BASE_Z;
    float magnitude = sqrtf(dx * dx + dy * dy + dz * dz);

    if (magnitude >= TAP_ACCEL_THRESHOLD) {
        if (!s_in_spike) {
            s_in_spike     = true;
            s_spike_cycles = 0;
        }
        s_spike_cycles++;
    }
    else if (s_in_spike) {
        s_in_spike = false;
        if (s_spike_cycles <= TAP_MAX_CYCLES) {
            return IMU_EVENT_TAP;
        }
    }

    return -1;
}

// 搖晃偵測：X 軸在視窗內持續來回擺動
static int detect_shake(const mpu6050_accel_data_axes_t *accel)
{
    typedef enum { SHAKE_IDLE, SHAKE_DETECTING, SHAKE_SHAKING } shake_state_t;
    static shake_state_t s_state        = SHAKE_IDLE;
    static uint32_t      s_window_count = 0;
    static uint32_t      s_quiet_count  = 0;
    static uint32_t      s_consec_quiet = 0;

    float x = fmaxf(fabsf(accel->x_axis) - IMU_BASE_X, 0.0f);

    switch (s_state) {
        case SHAKE_IDLE:
            if (x > SHAKE_ENTRY_THRESHOLD) {
                s_state        = SHAKE_DETECTING;
                s_window_count = 0;
                s_quiet_count  = 0;
            }
            break;

        case SHAKE_DETECTING:
            s_window_count++;
            if (x < SHAKE_LOW_THRESHOLD) s_quiet_count++;
            if (s_window_count >= SHAKE_DETECT_WINDOW) {
                if (s_quiet_count < SHAKE_MAX_QUIET_IN_WINDOW) {
                    s_state        = SHAKE_SHAKING;
                    s_consec_quiet = 0;
                    return IMU_EVENT_SHAKE_START;
                } else {
                    s_state = SHAKE_IDLE;
                }
            }
            break;

        case SHAKE_SHAKING:
            if (x < SHAKE_LOW_THRESHOLD) {
                s_consec_quiet++;
                if (s_consec_quiet >= SHAKE_EXIT_CONSECUTIVE) {
                    s_state = SHAKE_IDLE;
                    return IMU_EVENT_SHAKE_END;
                }
            } else {
                s_consec_quiet = 0;
            }
            break;
    }

    return -1;
}

// IMU 感測任務：定時讀取加速度並觸發事件
static void imu_task(void *arg)
{
    mpu6050_accel_data_axes_t accel;
    mpu6050_gyro_data_axes_t  gyro;
    float                     temperature;

    while (true) {
        esp_err_t ret = mpu6050_get_motion(s_mpu6050_handle, &gyro, &accel, &temperature);
        if (ret == ESP_OK) {
            imu_event_t event = -1;
            int tap   = detect_tap(&accel);
            int shake = detect_shake(&accel);
            if (tap   != -1) event = (imu_event_t)tap;
            if (shake != -1) event = (imu_event_t)shake;
            if (event != -1) {
                for (int i = 0; i < 4; i++) {
                    if (s_event_cbs[i] != NULL) s_event_cbs[i](event);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(IMU_POLL_INTERVAL_MS));
    }
}

// ======================================================================
// 公開 API
// ======================================================================
// 初始化 MPU6050 IMU 模組並啟動感測任務
esp_err_t module_imu_init(void)
{
    ESP_LOGI(TAG, "IMU 正在初始化");

    esp_err_t ret = mpu6050_device_init();
    if (ret != ESP_OK) return ret;

    xTaskCreate(imu_task, "imu_task", IMU_TASK_STACK_SIZE, NULL, IMU_TASK_PRIORITY, &s_imu_task_handle);

    ESP_LOGI(TAG, "IMU 初始化完成");
    return ESP_OK;
}

// 新增 IMU 事件 callback
void module_imu_add_callback(imu_event_cb_t cb)
{
    for (int i = 0; i < 4; i++) {
        if (s_event_cbs[i] == NULL) {
            s_event_cbs[i] = cb;
            return;
        }
    }
    ESP_LOGW(TAG, "callback 已滿，無法新增");
}

// 移除 IMU 事件 callback
void module_imu_remove_callback(imu_event_cb_t cb)
{
    for (int i = 0; i < 4; i++) {
        if (s_event_cbs[i] == cb) {
            s_event_cbs[i] = NULL;
            return;
        }
    }
}
