// ESP-IDF 系統組件
#include <esp_log.h>
#include <esp_err.h>

// 本專案模組
#include "module_boot.h"
#include "module_nvs.h"
#include "module_bus.h"

// 日誌標籤
static const char *TAG = "M_Boot";

// ======================================================================
// 公開 API
// ======================================================================
// 執行完整啟動流程
esp_err_t module_boot_run(void)
{
    
    // NVS 初始化
    esp_err_t ret = module_nvs_init();
    if (ret != ESP_OK) return ret;

    // 匯流排初始化
    ret = module_bus_init();
    if (ret != ESP_OK) return ret;

    // todo: 依序執行：NVS → 匯流排 → SD → 顯示初始化 → 開機動畫 → DS3231 → MPU6050 → INMP441 → MAX98357A

    ESP_LOGI(TAG, "=====系統啟動完成=====");
    return ESP_OK;
}