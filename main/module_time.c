// C 標準函式庫
#include <stdint.h>

// ESP-IDF 系統組件
#include <esp_err.h>
#include <esp_log.h>
#include <driver/i2c_master.h>

// 本專案模組
#include "board.h"
#include "module_bus.h"
#include "module_time.h"

// 日誌標籤
static const char *TAG = "M_Time";

// ======================================================================
// 私有巨集
// ======================================================================

#define DS3231_I2C_SPEED_HZ     400000  // DS3231 最高支援 Fast Mode (400kHz)

// DS3231 暫存器地址
#define DS3231_REG_SECONDS      0x00
#define DS3231_REG_MINUTES      0x01
#define DS3231_REG_HOURS        0x02
#define DS3231_REG_DAY          0x03
#define DS3231_REG_DATE         0x04
#define DS3231_REG_MONTH        0x05
#define DS3231_REG_YEAR         0x06
#define DS3231_REG_STATUS       0x0F
#define DS3231_REG_TEMP_MSB     0x11
#define DS3231_REG_TEMP_LSB     0x12
#define DS3231_OSF_BIT          (1 << 7)  // Oscillator Stop Flag：斷電或晶振停止時被設為 1

// ======================================================================
// 私有變數
// ======================================================================

static i2c_master_dev_handle_t s_ds3231_dev;                          // DS3231 I2C 裝置 handle
static module_time_status_t    s_status = MODULE_TIME_STATUS_RTC_ERROR; // 預設為故障，init 成功後更新

// ======================================================================
// 私有函式實作
// ======================================================================

static uint8_t bcd_to_dec(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
static uint8_t dec_to_bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

// ======================================================================
// 公開 API
// ======================================================================

// 初始化 DS3231
esp_err_t module_time_init(void)
{
    ESP_LOGI(TAG, "DS3231 正在初始化");

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = CONFIG_DS3231_ADDR,
        .scl_speed_hz    = DS3231_I2C_SPEED_HZ,
    };

    esp_err_t ret = i2c_master_bus_add_device(module_bus_i2c0_handle(), &dev_cfg, &s_ds3231_dev);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "DS3231 掛載至 I2C0 失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    // 探測裝置確認通訊正常
    ret = i2c_master_probe(module_bus_i2c0_handle(), CONFIG_DS3231_ADDR, 100);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "DS3231 未回應（位址 0x%02X）: %s", CONFIG_DS3231_ADDR, esp_err_to_name(ret));
        return ret;
    }

    // 讀取 Status Register，檢查 OSF（Oscillator Stop Flag）
    uint8_t status_reg_addr = DS3231_REG_STATUS;
    uint8_t status_val = 0;
    ret = i2c_master_transmit_receive(s_ds3231_dev, &status_reg_addr, 1, &status_val, 1, 100);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Status Register 讀取失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    if (status_val & DS3231_OSF_BIT)
    {
        ESP_LOGW(TAG, "OSF 已設定，RTC 曾發生斷電或晶振停止，時間不可信");
        s_status = MODULE_TIME_STATUS_POWER_LOSS;
        // 清除 OSF
        uint8_t clear_osf[2] = { DS3231_REG_STATUS, status_val & ~DS3231_OSF_BIT };
        ret = i2c_master_transmit(s_ds3231_dev, clear_osf, sizeof(clear_osf), 100);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "OSF 清除失敗: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    else
    {
        s_status = MODULE_TIME_STATUS_OK;
    }

    ESP_LOGI(TAG, "DS3231 初始化成功（位址 0x%02X）", CONFIG_DS3231_ADDR);
    return ESP_OK;
}

// 查詢 RTC 狀態
module_time_status_t module_time_get_status(void)
{
    return s_status;
}

// 讀取 DS3231 當前時間
esp_err_t module_time_get(module_time_t *out)
{
    uint8_t buf[7] = {0};

    uint8_t reg = DS3231_REG_SECONDS;
    esp_err_t ret = i2c_master_transmit_receive(s_ds3231_dev, &reg, 1, buf, sizeof(buf), 100);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "時間讀取失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    out->second  = bcd_to_dec(buf[0] & 0x7F);
    out->minute  = bcd_to_dec(buf[1] & 0x7F);
    out->hour    = bcd_to_dec(buf[2] & 0x3F);
    out->weekday = bcd_to_dec(buf[3] & 0x07);
    out->day     = bcd_to_dec(buf[4] & 0x3F);
    out->month   = bcd_to_dec(buf[5] & 0x1F);
    out->year    = bcd_to_dec(buf[6]) + 2000;

    return ESP_OK;
}

// 將時間寫入 DS3231
esp_err_t module_time_set(const module_time_t *t)
{
    uint8_t buf[8] = {
        DS3231_REG_SECONDS,
        dec_to_bcd(t->second),
        dec_to_bcd(t->minute),
        dec_to_bcd(t->hour),
        dec_to_bcd(t->weekday),
        dec_to_bcd(t->day),
        dec_to_bcd(t->month),
        dec_to_bcd(t->year - 2000),
    };
    esp_err_t ret = i2c_master_transmit(s_ds3231_dev, buf, sizeof(buf), 100);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "時間寫入失敗: %s", esp_err_to_name(ret));
        return ret;
    }
    s_status = MODULE_TIME_STATUS_OK;
    return ESP_OK;
}

// 讀取 DS3231 內建溫度感測器
esp_err_t module_time_get_temp(float *out)
{
    uint8_t reg = DS3231_REG_TEMP_MSB;
    uint8_t buf[2] = {0};
    esp_err_t ret = i2c_master_transmit_receive(s_ds3231_dev, &reg, 1, buf, 2, 100);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "溫度讀取失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    int8_t  integer = (int8_t)buf[0];
    uint8_t frac    = (buf[1] >> 6) & 0x03;  // bit 7-6：0.25°C 步進
    *out = integer + frac * 0.25f;
    return ESP_OK;
}
