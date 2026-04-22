// ESP-IDF 系統組件
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_heap_caps.h>

// 本專案模組
#include "module_boot.h"
#include "module_nvs.h"
#include "module_bus.h"
#include "module_sd.h"
#include "module_display.h"
#include "module_touch.h"
#include "module_lvgl.h"
#include "module_ui_boot.h"
#include "module_time.h"
#include "module_imu.h"
#include "module_pir.h"
#include "module_network.h"

// 日誌標籤
static const char *TAG = "M_Boot";

// ======================================================================
// 私有巨集
// ======================================================================
#define LOG_HEAP() \
    ESP_LOGI(TAG, "[Heap] SRAM 剩餘 %u / 最大塊 %u bytes | PSRAM 剩餘 %u / 最大塊 %u bytes", \
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL), \
             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), \
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM), \
             heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM))

// ======================================================================
// 私有函式
// ======================================================================
// GPIO3 為 ESP32-S3 JTAG 啟動配置引腳，控制 GPIO39–42 的 JTAG 功能是否啟用
// 避免 GPIO3 啟動時狀態無法確認導致 JTAG 狀態殘留，因此進行防禦性釋放
static void gpio_reset(void)
{
    gpio_reset_pin(GPIO_NUM_39);
    gpio_reset_pin(GPIO_NUM_40);
    gpio_reset_pin(GPIO_NUM_41);
    gpio_reset_pin(GPIO_NUM_42);
}
        
// ======================================================================
// 公開 API
// ======================================================================
// 執行完整啟動流程
esp_err_t module_boot_run(void)
{
    gpio_reset();
    LOG_HEAP();

    // NVS 初始化
    esp_err_t ret = module_nvs_init();
    if (ret != ESP_OK) return ret;
    LOG_HEAP();

    // 匯流排初始化
    ret = module_bus_init();
    if (ret != ESP_OK) return ret;
    LOG_HEAP();

    // SD 卡初始化
    ret = module_sd_init();
    if (ret != ESP_OK) return ret;
    LOG_HEAP();

    // 顯示器初始化
    ret = module_display_init();
    if (ret != ESP_OK) return ret;
    LOG_HEAP();

    // 觸控初始化
    ret = module_touch_init();
    if (ret != ESP_OK) return ret;
    LOG_HEAP();

    // LVGL 初始化
    ret = module_lvgl_init();
    if (ret != ESP_OK) return ret;
    LOG_HEAP();

    // 顯示開機畫面（含背光淡入與進度條淡入，阻塞）
    module_ui_boot_show();
    LOG_HEAP();
    
    // DS3231 初始化（非關鍵，失敗不中斷啟動）
    module_ui_boot_set_status("正在初始化時鐘模組...");
    module_time_init();
    module_ui_boot_set_progress(25);
    vTaskDelay(pdMS_TO_TICKS(500));
    LOG_HEAP();

    // IMU 初始化（非關鍵，失敗不中斷啟動）
    module_ui_boot_set_status("正在初始化運動感測器...");
    module_imu_init();
    module_ui_boot_set_progress(50);
    vTaskDelay(pdMS_TO_TICKS(500));
    LOG_HEAP();

    // PIR 初始化（非關鍵，失敗不中斷啟動）
    module_ui_boot_set_status("正在初始化人體感測器...");
    module_pir_init();
    module_ui_boot_set_progress(60);
    vTaskDelay(pdMS_TO_TICKS(200));
    LOG_HEAP();

    // 網路模組初始化（非關鍵，失敗不中斷啟動）
    module_ui_boot_set_status("正在初始化網路模組...");
    module_network_init();
    module_ui_boot_set_progress(70);
    vTaskDelay(pdMS_TO_TICKS(300));
    LOG_HEAP();

    // TODO: 兩個 i2s 裝置 init

    // 尚未實作前的假等待模擬
    module_ui_boot_set_status("系統初始化完成");
    module_ui_boot_set_progress(100);
    module_ui_boot_set_complete();
    vTaskDelay(pdMS_TO_TICKS(1200));
    return ESP_OK;
}