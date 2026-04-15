# 貢獻指南

## 快速構建開發環境

1. 下載並安裝 [Git](https://git-scm.com/)
2. 下載並安裝 [VS Code](https://code.visualstudio.com)
3. 安裝 [ESP-IDF 擴充套件](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension)
4. 打開 ESP-IDF 安裝管理器，安裝 **ESP-IDF v5.5.3**
5. Clone 專案

```bash
git clone https://github.com/irukatun/Lumina.git
```

## Commit 命名規則

遵循 [Conventional Commits](https://www.conventionalcommits.org/) 規範。

```
<type>(<scope>): <簡述>

<詳細說明（選填）>
```

- **type**：本次變更的類別（必填）
- **scope**：影響範圍，通常是模組名稱，例如 `display`、`sd`、`wifi`（選填）
- **簡述**：一行說明做了什麼（必填）
- **詳細說明**：條列實作細節（選填，與標題空一行）

---

### Type 列表

| type | 用途 |
|------|------|
| `feat` | 新增功能 |
| `fix` | 修復 bug |
| `refactor` | 重構（不改變功能） |
| `perf` | 效能優化 |
| `style` | 格式調整（空白、縮排，不影響邏輯） |
| `docs` | 文件變更 |
| `test` | 新增或修改測試 |
| `chore` | 建構設定、工具鏈、依賴更新等雜項 |

## 代碼風格規定
> 目前未紀錄的命名規則一律以可讀性為優先命名考量

### 文件命名規則
本專案所有模組(一套.c.h視為一個模組)一律以`module_<功能>`命名。

---
### 函式命名規則
#### 公開 API 一律以 `module_<模組名>_<功能>`
範例: `module_boot_run` `module_nvs_init`

---
### 變數命名規則
#### 私有變數需帶 `s_` 前綴
範例: `s_nvs_reset` `s_firmware_updated`

---
### 巨集命名規則
#### 全域巨集一律定義於 `board.h`
全域巨集包含 **GPIO 腳位定義** 與 **硬體規格參數** 等大部分模組可能會多次引用且固定的配置

命名規則如未特別在代碼以註解定義一律以 `CONFIG_<零件>_<參數>` 為主

> 巨集一律使用全大寫命名

---
### 代碼實作分隔
所有代碼依照以下格式排序實作並註解  
[範例.c格式](docs/example.c.md)  
[範例.h格式](docs/example.h.md)  

---
### 禁止間接 include
用到什麼就直接 include 什麼（Include What You Use），不可間接透過其他模組或自身模組的標頭檔 include 組件或是模組
>　按照 [範例.h格式](docs/example.h.md)  使用 **#pragma once** 確保不會重複引用

---
### 分隔線統一為 70 個 "="
```
// ======================================================================
```