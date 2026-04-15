// ESP-IDF 系統組件
#include <esp_log.h>
#include <esp_err.h>
#include <esp_vfs_fat.h>
#include <driver/sdspi_host.h>
#include <driver/spi_common.h>
#include <sdmmc_cmd.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// 本專案模組
#include "board.h"
#include "module_sd.h"

static const char *TAG = "M_SD";

// ======================================================================
// 私有巨集
// ======================================================================

#define SD_FREQ_KHZ             20000   // SPI 時脈頻率
#define SD_MAX_OPEN_FILES       5       // VFS 同時開啟檔案數上限
#define SD_MOUNT_RETRY_COUNT    3       // 掛載重試次數
#define SD_MOUNT_RETRY_DELAY_MS 100     // 掛載重試間隔（ms）

// ======================================================================
// 私有變數
// ======================================================================

static sdmmc_card_t *s_card = NULL;

// ======================================================================
// 公開 API
// ======================================================================

// SD 卡掛載
esp_err_t module_sd_init(void)
{
    ESP_LOGI(TAG, "SD 正在初始化");

    const esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files              = SD_MAX_OPEN_FILES,
    };

    sdmmc_host_t host  = SDSPI_HOST_DEFAULT();
    host.slot          = SPI3_HOST;
    host.max_freq_khz  = SD_FREQ_KHZ;

    sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_cfg.gpio_cs = CONFIG_GPIO_SD_CS;
    slot_cfg.host_id = SPI3_HOST;

    esp_err_t ret = ESP_FAIL;
    for (int attempt = 1; attempt <= SD_MOUNT_RETRY_COUNT; attempt++) {
        ESP_LOGI(TAG, "SD 正在嘗試掛載（第 %d/%d 次）", attempt, SD_MOUNT_RETRY_COUNT);

        if (attempt > 1) {
            vTaskDelay(pdMS_TO_TICKS(SD_MOUNT_RETRY_DELAY_MS));
        }

        ret = esp_vfs_fat_sdspi_mount(CONFIG_SD_PATH, &host, &slot_cfg, &mount_cfg, &s_card);

        if (ret == ESP_OK) break;

        s_card = NULL;
        if (attempt < SD_MOUNT_RETRY_COUNT) {
            ESP_LOGW(TAG, "SD 掛載錯誤（第 %d 次）: %s", attempt, esp_err_to_name(ret));
        }
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD 掛載失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    sdmmc_card_print_info(stdout, s_card);
    ESP_LOGI(TAG, "SD 已掛載至 %s", CONFIG_SD_PATH);
    return ESP_OK;
}

// SD 卡卸載
esp_err_t module_sd_deinit(void)
{
    if (s_card == NULL) return ESP_OK;

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(CONFIG_SD_PATH, s_card);
    s_card = NULL;

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD 卸載失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SD 已卸載");
    return ESP_OK;
}

// SD 卡裝置資訊
const sdmmc_card_t *module_sd_info(void)
{
    return s_card;
}
