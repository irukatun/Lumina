// ESP-IDF 系統組件
#include <esp_log.h>
#include <esp_err.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// 本專案模組
#include "board.h"
#include "module_pir.h"

// 日誌標籤
static const char *TAG = "M_PIR";

// ======================================================================
// 私有巨集
// ======================================================================
#define PIR_TASK_STACK_SIZE     2048
#define PIR_TASK_PRIORITY       5

// ======================================================================
// 私有變數
// ======================================================================
static pir_event_cb_t   s_event_cbs[4]      = {NULL};
static TaskHandle_t     s_pir_task_handle    = NULL;
static volatile bool    s_detected           = false;

// ======================================================================
// 私有函式前向宣告
// ======================================================================
static void pir_isr_handler(void *arg);
static void pir_task(void *arg);

// ======================================================================
// 私有函式實作
// ======================================================================
static void IRAM_ATTR pir_isr_handler(void *arg)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_pir_task_handle, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

static void pir_task(void *arg)
{
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        s_detected = gpio_get_level(CONFIG_GPIO_PIR_OUTPUT);
        pir_event_t event = s_detected ? PIR_EVENT_DETECTED : PIR_EVENT_CLEARED;
        for (int i = 0; i < 4; i++) {
            if (s_event_cbs[i] != NULL) s_event_cbs[i](event);
        }
    }
}

// ======================================================================
// 公開 API
// ======================================================================
// 初始化 PIR 模組並啟動中斷監聽
esp_err_t module_pir_init(void)
{
    ESP_LOGI(TAG, "PIR 正在初始化");

    const gpio_config_t io_conf = {
        .pin_bit_mask   = (1ULL << CONFIG_GPIO_PIR_OUTPUT),
        .mode           = GPIO_MODE_INPUT,
        .pull_up_en     = GPIO_PULLUP_DISABLE,
        .pull_down_en   = GPIO_PULLDOWN_DISABLE,
        .intr_type      = GPIO_INTR_ANYEDGE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO 設定失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    xTaskCreate(pir_task, "pir_task", PIR_TASK_STACK_SIZE, NULL, PIR_TASK_PRIORITY, &s_pir_task_handle);

    // ESP-IDF 在已安裝時會無條件印出 error log 再回傳 INVALID_STATE，無法從外部抑制
    // 此為已知行為，INVALID_STATE 代表其他模組（如 esp_lcd_touch）已安裝，可安全忽略
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "ISR 服務安裝失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = gpio_isr_handler_add(CONFIG_GPIO_PIR_OUTPUT, pir_isr_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ISR handler 安裝失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    s_detected = gpio_get_level(CONFIG_GPIO_PIR_OUTPUT);
    ESP_LOGI(TAG, "PIR 初始化完成， INVALID_STATE 代表其他模組已安裝，可安全忽略");
    return ESP_OK;
}

bool module_pir_is_detected(void)
{
    return s_detected;
}

// 新增 PIR 事件 callback
void module_pir_add_callback(pir_event_cb_t cb)
{
    for (int i = 0; i < 4; i++) {
        if (s_event_cbs[i] == NULL) {
            s_event_cbs[i] = cb;
            return;
        }
    }
    ESP_LOGW(TAG, "callback 已滿，無法新增");
}

// 移除 PIR 事件 callback
void module_pir_remove_callback(pir_event_cb_t cb)
{
    for (int i = 0; i < 4; i++) {
        if (s_event_cbs[i] == cb) {
            s_event_cbs[i] = NULL;
            return;
        }
    }
}
