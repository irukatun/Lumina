#pragma once

// ESP-IDF 系統組件
#include <esp_err.h>
#include <sdmmc_cmd.h>

/**
 * @brief 初始化 SD 卡模組並以 SPI 模式掛載 FAT 檔案系統
 *
 * @return ESP_OK 成功；其他值表示掛載過程存在致命錯誤，呼叫端應重啟
 */
esp_err_t module_sd_init(void);

/**
 * @brief 卸載 SD 卡並釋放資源
 *
 * @return ESP_OK 成功；其他值表示卸載過程存在致命錯誤，呼叫端應重啟
 */
esp_err_t module_sd_deinit(void);

/**
 * @brief 取得 SD 卡裝置資訊
 *
 * @return 暴露 sdmmc_card_t 指標內容
 */
const sdmmc_card_t *module_sd_info(void);
