# 貢獻指南

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