// ESP-IDF 系統標頭
#include <esp_log.h>
#include <esp_err.h>

// ESP-IDF 驅動
#include <driver/spi_common.h>
#include <driver/i2c_master.h>

// 本專案模組
#include "board.h"
#include "module_bus.h"

static const char *TAG = "M_bus";

// ======================================================================
// 私有巨集
// ======================================================================

#define SPI2_MAX_TRANSFER_SZ    46080   // 對齊 ILI9488 480寬×32行×RGB888
#define SPI3_MAX_TRANSFER_SZ    32768   // 對齊 SD FAT32 32KB cluster（64 扇區）

// ======================================================================
// 私有變數
// ======================================================================

static i2c_master_bus_handle_t s_i2c0_bus;

// ======================================================================
// 私有函式前向宣告
// ======================================================================

static esp_err_t spi2_bus_init(void);
static esp_err_t spi3_bus_init(void);
static esp_err_t i2c0_bus_init(void);

// ======================================================================
// 私有函式實作
// ======================================================================
// spi2 匯流排初始化
static esp_err_t spi2_bus_init(void)
{
    ESP_LOGI(TAG, "SPI2-IOMUX 匯流排正在初始化");

    const spi_bus_config_t bus_cfg = {
        .mosi_io_num     = CONFIG_GPIO_SPI2_MOSI,
        .miso_io_num     = CONFIG_GPIO_SPI2_MISO,
        .sclk_io_num     = CONFIG_GPIO_SPI2_CLOCK,
        .quadwp_io_num   = GPIO_NUM_NC,
        .quadhd_io_num   = GPIO_NUM_NC,
        .max_transfer_sz = SPI2_MAX_TRANSFER_SZ,
        .flags           = SPICOMMON_BUSFLAG_SCLK | SPICOMMON_BUSFLAG_MISO | SPICOMMON_BUSFLAG_MOSI | SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_IOMUX_PINS,
        .intr_flags      = ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM,
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI2-IOMUX 匯流排初始化失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SPI2-IOMUX 匯流排初始化成功");
    return ESP_OK;
}
// spi3 匯流排初始化
static esp_err_t spi3_bus_init(void)
{
    ESP_LOGI(TAG, "SPI3 匯流排正在初始化");

    const spi_bus_config_t bus_cfg = {
        .mosi_io_num     = CONFIG_GPIO_SPI3_MOSI,
        .miso_io_num     = CONFIG_GPIO_SPI3_MISO,
        .sclk_io_num     = CONFIG_GPIO_SPI3_CLK,
        .quadwp_io_num   = GPIO_NUM_NC,
        .quadhd_io_num   = GPIO_NUM_NC,
        .max_transfer_sz = SPI3_MAX_TRANSFER_SZ,
    };

    esp_err_t ret = spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI3 匯流排初始化失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SPI3 匯流排初始化成功");
    return ESP_OK;
}
// i2c0 匯流排初始化
static esp_err_t i2c0_bus_init(void)
{
    ESP_LOGI(TAG, "I2C0 匯流排正在初始化");

    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port            = I2C_NUM_0,
        .sda_io_num          = CONFIG_GPIO_I2C0_SDA,
        .scl_io_num          = CONFIG_GPIO_I2C0_SCL,
        .clk_source          = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt   = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &s_i2c0_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C0 匯流排初始化失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2C0 匯流排初始化成功");
    return ESP_OK;
}

// ======================================================================
// 公開 API
// ======================================================================
// 匯流排初始化
esp_err_t module_bus_init(void)
{
    esp_err_t ret = spi2_bus_init();
    if (ret != ESP_OK) return ret;

    ret = spi3_bus_init();
    if (ret != ESP_OK) return ret;

    ret = i2c0_bus_init();
    if (ret != ESP_OK) return ret;

    return ESP_OK;
}
// I2C0 匯流排 handle
i2c_master_bus_handle_t module_bus_i2c0_handle(void)
{
    return s_i2c0_bus;
}