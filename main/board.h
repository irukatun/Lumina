#pragma once

#include <driver/gpio.h>

// ======================================================================
// [韌體版本]
//   格式：主版本.次版本（大功能更新.小功能實作）
//   推送至 main 分支請按照以下規則:
//   主次版本統一由主要維護者 irukatun 設定，測試時可自定義加 build (-bxx) 後綴
// ======================================================================
#define FIRMWARE_VERSION "0.0-b0"

// ======================================================================
// [GPIO 腳位定義]
//   格式：CONFIG_GPIO_<共用協議/零件>_<接口>
// ======================================================================
// SPI2 (FSPI) IOMUX -> ILI9488 XPT2046
#define CONFIG_GPIO_SPI2_CLOCK            GPIO_NUM_12     // FSPICLK (IOMUX)
#define CONFIG_GPIO_SPI2_MOSI             GPIO_NUM_11     // FSPID   (IOMUX)
#define CONFIG_GPIO_SPI2_MISO             GPIO_NUM_13     // FSPIQ   (IOMUX)
#define CONFIG_GPIO_ILI9488_CS            GPIO_NUM_10     // FSPICS0 (IOMUX) 
#define CONFIG_GPIO_ILI9488_RESET         GPIO_NUM_46
#define CONFIG_GPIO_ILI9488_DC            GPIO_NUM_9
#define CONFIG_GPIO_ILI9488_BACKLIGHT     GPIO_NUM_14
#define CONFIG_GPIO_XPT2046_CS            GPIO_NUM_3
#define CONFIG_GPIO_XPT2046_IRQ           GPIO_NUM_8
// SPI3 (HSPI) Matrix -> SD
#define CONFIG_GPIO_SPI3_CLK              GPIO_NUM_1
#define CONFIG_GPIO_SPI3_MOSI             GPIO_NUM_2
#define CONFIG_GPIO_SPI3_MISO             GPIO_NUM_42
#define CONFIG_GPIO_SD_CS                 GPIO_NUM_4
// I2C0 -> DS3231 MPU6050 ( MPU6050 AD0 拉高 )
#define CONFIG_GPIO_I2C0_SDA              GPIO_NUM_17
#define CONFIG_GPIO_I2C0_SCL              GPIO_NUM_18
#define CONFIG_GPIO_MPU6050_INT           GPIO_NUM_16
// I2S0 -> INMP441
#define CONFIG_GPIO_I2S0_WS               GPIO_NUM_5
#define CONFIG_GPIO_I2S0_SCK              GPIO_NUM_6
#define CONFIG_GPIO_I2S0_SD               GPIO_NUM_7
// I2S1 -> MAX98357A
#define CONFIG_GPIO_I2S1_BCLK             GPIO_NUM_40
#define CONFIG_GPIO_I2S1_LRC              GPIO_NUM_41
#define CONFIG_GPIO_I2S1_DIN              GPIO_NUM_39
// PIR
#define CONFIG_GPIO_PIR_OUTPUT            GPIO_NUM_15

// ======================================================================
// 硬體規格參數
// ======================================================================
// ILI9488
#define CONFIG_ILI9488_WIDTH              480
#define CONFIG_ILI9488_HEIGHT             320
// I2C 地址
#define CONFIG_DS3231_ADDR                0x68
#define CONFIG_MPU6050_ADDR               0x69            // AD0 拉高 0x68->0x69