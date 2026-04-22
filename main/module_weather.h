#pragma once

#include <esp_err.h>
#include "module_sync.h"

// ======================================================================
// 公開 API
// ======================================================================

/**
 * @brief 向 QWeather 取得即時天氣並填入結構
 *
 * 內部流程：Geo lookup → Now API → gzip 解壓（如需）→ JSON 解析
 * 純後端邏輯，不碰 UI。
 *
 * @param[in]  city  城市名稱（傳入 CONFIG_WEATHER_DEFAULT_CITY）
 * @param[out] out   成功時填入天氣資料
 * @return ESP_OK 成功；其他為失敗
 */
esp_err_t module_weather_fetch(module_sync_weather_t *out);
