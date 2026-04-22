// ESP-IDF 系統組件
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// 本專案模組
#include "module_boot.h"
#include "module_sync.h"
#include "module_ui_main.h"
#include "module_ui_network.h"

// 日誌標籤
static const char *TAG = "Main";

void app_main(void)
{
    ESP_LOGI(TAG, "=====系統啟動中=====");
    if (module_boot_run() != ESP_OK)
    {
        ESP_LOGE(TAG, "系統啟動失敗，10秒後重新啟動");
        vTaskDelay(pdMS_TO_TICKS(10000)); // 等待 10 秒以便觀察錯誤訊息
        esp_restart();
    }
    module_ui_network_show();
    vTaskDelay(pdMS_TO_TICKS(1000));
    module_sync_init();
    module_ui_main_show();
    ESP_LOGI(TAG, "=====系統啟動完成=====");
}
