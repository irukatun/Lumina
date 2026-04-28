#pragma once

#include <stdbool.h>
#include "module_alarm.h"

/**
 * @brief 鬧鐘編輯完成 callback
 *
 * @param saved  true=使用者點儲存；false=取消
 * @param result 使用者填入的鬧鐘資料（saved=false 時無效）
 * @param slot   原始 slot index（-1 表示新增）
 */
typedef void (*alarm_edit_done_cb_t)(bool saved, const alarm_t *result, int slot);

/**
 * @brief 開啟鬧鐘編輯對話框
 *
 * 以 overlay 方式疊在目前畫面上，提供時/分 roller 和星期幾 toggle 按鈕。
 * 重複呼叫沒有作用。
 *
 * @param slot    -1 表示新增；0-9 表示編輯現有 slot
 * @param initial 初始值（NULL 時預設 08:00、無重複）
 * @param cb      完成 callback
 */
void module_ui_alarm_edit_open(int slot, const alarm_t *initial, alarm_edit_done_cb_t cb);

/**
 * @brief 關閉鬧鐘編輯對話框並釋放資源
 */
void module_ui_alarm_edit_close(void);
