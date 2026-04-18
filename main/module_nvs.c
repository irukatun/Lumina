// C 標準函式庫
#include <string.h>
#include <stdbool.h>

// ESP-IDF 系統組件
#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>

// 本專案模組
#include "module_nvs.h"
#include "board.h"

static const char *TAG = "M_NVS";

// ======================================================================
// 私有變數
// ======================================================================

static bool s_nvs_reset = false; // NVS 因損毀而被自動清除的旗標
static bool s_ver_updated = false; // 本次開機偵測到韌體版本更新的旗標

// ======================================================================
// 私有函式前向宣告
// ======================================================================

static esp_err_t check_firmware_version(bool *is_new);

// ======================================================================
// 私有函式實作
// ======================================================================

// 檢查並更新 NVS 中的韌體版本號
static esp_err_t check_firmware_version(bool *is_new)
{
    *is_new = false;

    nvs_handle_t handle;
    esp_err_t ret = nvs_open("firmware", NVS_READWRITE, &handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "[版本比對]無法開啟 firmware namespace: %s", esp_err_to_name(ret));
        return ret;
    }

    char stored[32] = {0};
    size_t len = sizeof(stored);
    ret = nvs_get_str(handle, "ver", stored, &len);

    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "[版本比對]無歷史版本號");
        *is_new = true;
    }
    else if (ret == ESP_OK)
    {
        *is_new = (strcmp(stored, FIRMWARE_VERSION) != 0);
        if (*is_new)
            ESP_LOGI(TAG, "[版本比對]偵測到韌體更新 (%s → %s)", stored, FIRMWARE_VERSION);
        else
            ESP_LOGI(TAG, "[版本比對]韌體版本未變更 (%s)", FIRMWARE_VERSION);
    }
    else
    {
        ESP_LOGE(TAG, "[版本比對]版本號讀取失敗: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }

    if (*is_new)
    {
        ret = nvs_set_str(handle, "ver", FIRMWARE_VERSION);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "[版本比對]版本號寫入暫存失敗: %s", esp_err_to_name(ret));
            nvs_close(handle);
            return ret;
        }
        ret = nvs_commit(handle);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "[版本比對]版本號存入 NVS 失敗: %s", esp_err_to_name(ret));
            nvs_close(handle);
            return ret;
        }
        ESP_LOGI(TAG, "[版本比對]版本號已更新至 %s", FIRMWARE_VERSION);
    }

    nvs_close(handle);
    return ESP_OK;
}

// ======================================================================
// 公開 API
// ======================================================================

// 初始化 NVS
esp_err_t module_nvs_init(void)
{
    ESP_LOGI(TAG, "NVS 正在初始化");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "分區損毀，正在清除重建");
        ret = nvs_flash_erase();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "分區清除失敗: %s", esp_err_to_name(ret));
            return ret;
        }
        ret = nvs_flash_init();
        s_nvs_reset = true;
    }
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "初始化失敗: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "NVS 初始化成功");

    esp_err_t fw_ret = check_firmware_version(&s_ver_updated);
    if (fw_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "版本號對比更新失敗: %s", esp_err_to_name(fw_ret));
        return fw_ret;
    }
    return ESP_OK;
}

// 讀取指定 namespace / key 的 u8 值
esp_err_t module_nvs_get_u8(const char *ns, const char *key, uint8_t *out)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(ns, NVS_READONLY, &handle);
    if (ret != ESP_OK) return ret;
    ret = nvs_get_u8(handle, key, out);
    nvs_close(handle);
    return ret;
}

// 寫入指定 namespace / key 的 u8 值
esp_err_t module_nvs_set_u8(const char *ns, const char *key, uint8_t value)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(ns, NVS_READWRITE, &handle);
    if (ret != ESP_OK) return ret;
    ret = nvs_set_u8(handle, key, value);
    if (ret == ESP_OK) ret = nvs_commit(handle);
    nvs_close(handle);
    return ret;
}

// 供外部查詢版本是否更新的 API
bool module_nvs_is_ver_updated(void)
{
    return s_ver_updated;
}
// 供外部查詢 NVS 是否被重置的 API
bool module_nvs_is_reset(void)
{
    return s_nvs_reset;
}
