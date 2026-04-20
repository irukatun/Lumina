# 字體準備
本專案全部使用 [jf open 粉圓字體](https://github.com/justfont/open-huninn-font) 進行實作，可以請按照以下方式快速建立所需字體庫。
## 前置需求

- Node.js（任意版本）
- lv_font_conv >= 1.5.0

```bash
npm install -g lv_font_conv  
lv_font_conv --version  # 確認 >= 1.5.0
```

## 生成字體

在專案根目錄執行：

```bash
node main/fonts/generate_font.js
```

字體來源（`.ttf`）不存在時會自動從 GitHub 下載，無需手動準備。

## 新增中文字

編輯 `main/fonts/symbols.txt`，將 UI 需要的中文字填入（非 `#` 開頭的行），重新執行腳本即可。

ASCII 字元（32–127）已自動包含，無需手動列出。
