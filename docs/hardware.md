# 硬體規格與接線

## 使用到的零件

| 類別 | 元件 |
|------|------|
| 主控 | ESP32-S3-N16R8-WROOM-1 |
| 協處理器 | Orange Pi One 512M（尚未確定）|
| 攝像頭 | 待定 |
| 螢幕 | ILI9488（含 XPT2046 觸控） |
| 慣性感測器 | MPU-6050 |
| 麥克風 | INMP441 |
| 功率放大器 | MAX98357A |
| 揚聲器 | 3W 4Ω 全頻小型揚聲器 |
| 即時時鐘 | DS3231 |
| 儲存 | SanDisk Ultra 32GB（FAT32 / 32KB cluster） |
| 人體感測 | HC-SR505 PIR |
| 輸入 | 微動感測器 |

## 電源接線圖

```
升降壓模組 (5.15V)
│
├───── Orange Pi One
├───── ESP32-S3 (3.3V)
│              │
│              ├───── ILI9488
│              │        ├─XPT2046
│              ├───── DS3231
│              ├───── MPU6050
│              ├───── INMP441
│
├───── MAX98357A ── Speaker 3W/4ohm
├───── SD Card
├───── PIR
```
> **請根據你所使用的零件官方手冊選擇電壓**，因此專案使用的 ILI9488 LOD 損毀，因此短接 C1/J1 需要使用 3.3V 避免燒毀，正常購買可以使用 5V 供電。

## GPIO 接線圖

| ILI9488 | GPIO |
|-------------|------|
| CLK | GPIO 12 |
| MOSI | GPIO 11 |
| MISO | GPIO 13 |
| CS | GPIO 10 |
| RST | GPIO 46 |
| DC | GPIO 9 |
| BL | GPIO 14 |

| XPT2046 | GPIO |
|-------------|------|
| CLK | GPIO 12 |
| MOSI | GPIO 11 |
| MISO | GPIO 13 |
| CS | GPIO 3 |
| IRQ | GPIO 8 |

| SD | GPIO |
|--------|------|
| CLK | GPIO 1 |
| MOSI | GPIO 2 |
| MISO | GPIO 42 |
| CS | GPIO 4 |

| DS3231 | GPIO |
|-----------|------|
| SDA | GPIO 17 |
| SCL | GPIO 18 |

| MPU6050 | GPIO |
|------------|------|
| SDA | GPIO 17 |
| SCL | GPIO 18 |
| INT | GPIO 16 |
> AD0 拉高，I2C 位址為 0x69

| INMP441 | GPIO |
|------------|------|
| WS | GPIO 5 |
| SCK | GPIO 6 |
| SD | GPIO 7 |

| MAX98357A | GPIO |
|--------------|------|
| BCLK | GPIO 40 |
| LRC | GPIO 41 |
| DIN | GPIO 39 |

| HC-SR505 | GPIO |
|-------------|------|
| OUT | GPIO 15 |