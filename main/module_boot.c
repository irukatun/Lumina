// ESP-IDF 系統組件
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
// 公開 API
// ======================================================================
// 執行完整啟動流程
esp_err_t module_boot_run(void)
{
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

    // 顯示開機畫面
    module_ui_boot_show();
    module_display_backlight_set(100);
    LOG_HEAP();
    
    // DS3231 初始化
    ret = module_time_init();
    if (ret != ESP_OK) return ret;
    module_ui_boot_set_progress(25);
    LOG_HEAP();

    // TODO: 以下各模組加入後，set_progress 移至各 init 完成後
    // MPU6050:  module_mpu6050_init();  module_ui_boot_set_progress(50);
    // INMP441:  module_inmp441_init();  module_ui_boot_set_progress(75);
    // MAX98357: module_max98357_init(); module_ui_boot_set_progress(100);

    // 尚未實作前的假等待模擬
    vTaskDelay(pdMS_TO_TICKS(600));
    module_ui_boot_set_progress(55);
    vTaskDelay(pdMS_TO_TICKS(400));
    module_ui_boot_set_progress(75);
    vTaskDelay(pdMS_TO_TICKS(100));
    module_ui_boot_set_progress(100);
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "=====系統啟動完成=====");
    return ESP_OK;
}