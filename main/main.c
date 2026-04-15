// ESP-IDF 系統組件
#include <esp_log.h>
#include <driver/gpio.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// 本專案模組
#include "module_boot.h"

// 日誌標籤
static const char *TAG = "Main"; 

void app_main(void)
{
    ESP_LOGI(TAG, "=====系統啟動中=====");

    // GPIO3 為 ESP32-S3 JTAG 啟動配置引腳，控制 GPIO39–42 的 JTAG 功能是否啟用
    // 避免 GPIO3 啟動時狀態無法確認導致 JTAG 狀態殘留，因此進行防禦性釋放
    gpio_reset_pin(GPIO_NUM_39);
    gpio_reset_pin(GPIO_NUM_40);
    gpio_reset_pin(GPIO_NUM_41);
    gpio_reset_pin(GPIO_NUM_42);

    // 執行完整啟動
    if (module_boot_run() != ESP_OK)
    {
        ESP_LOGE(TAG, "系統啟動失敗，10秒後重新啟動");
        vTaskDelay(pdMS_TO_TICKS(10000)); // 等待 10 秒以便觀察錯誤訊息
        esp_restart();
    }

    // todo: 主畫面循環
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}
