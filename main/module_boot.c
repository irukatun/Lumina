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
#include "module_imu.h"
#include "module_pir.h"
#include "module_wifi.h"
#include "module_prov.h"
#include "module_network.h"
#include "module_ui_network.h"

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
    module_wifi_init();
    module_prov_init();
    module_network_init();
    module_ui_boot_set_progress(70);
    vTaskDelay(pdMS_TO_TICKS(300));
    LOG_HEAP();

    // TODO: 以下各模組加入後，set_progress 移至各 init 完成後
    // INMP441:  module_inmp441_init();  module_ui_boot_set_progress(85);
    // MAX98357: module_max98357_init(); module_ui_boot_set_progress(100);

    // 尚未實作前的假等待模擬
    module_ui_boot_set_status("正在初始化音訊模組...");
    module_ui_boot_set_progress(75);
    vTaskDelay(pdMS_TO_TICKS(700));
    module_ui_boot_set_status("正在載入主介面...");
    module_ui_boot_set_progress(95);
    vTaskDelay(pdMS_TO_TICKS(2000));
    module_ui_boot_set_status("啟動完成");
    module_ui_boot_set_progress(100);
    vTaskDelay(pdMS_TO_TICKS(500));
    module_ui_boot_set_complete();
    vTaskDelay(pdMS_TO_TICKS(1200));

    // 網路設定畫面（每次開機皆顯示，讓用戶選擇憑證或配對）
    // WiFi 連線由 UI 內的按鈕 handler 觸發，此處不再主動呼叫 module_wifi_start
    module_ui_network_show();

    ESP_LOGI(TAG, "=====系統啟動完成=====");
    return ESP_OK;
}