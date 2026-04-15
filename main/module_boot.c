// ESP-IDF 系統組件
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

    // 顯示開機動畫（待 module_ui_boot 實作完成後啟用）
    // module_ui_boot_show();

    // todo: 依序執行：NVS → 匯流排 → SD → 顯示初始化 → 開機動畫 → DS3231 → MPU6050 → INMP441 → MAX98357A

    ESP_LOGI(TAG, "=====系統啟動完成=====");
    return ESP_OK;
}