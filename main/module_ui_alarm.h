#pragma once

#include <stdint.h>

/**
 * @brief 以 overlay 方式在目前 active screen 上開啟鬧鐘設定面板
 *
 * 會建立一層半透明黑色背板與置中面板，背景主畫面不被銷毀、時鐘持續更新。
 * 重複呼叫沒有作用。
 */
void module_ui_alarm_open(void);

/**
 * @brief 關閉鬧鐘設定面板並釋放 overlay 資源
 */
void module_ui_alarm_close(void);

/**
 * @brief 顯示鬧鐘觸發 overlay（全螢幕）
 *
 * 在 alarm_task context 中呼叫，需在 lvgl_port_lock 保護下執行。
 * 重複呼叫沒有作用。
 */
void module_ui_alarm_trigger_open(uint8_t hour, uint8_t minute);

/**
 * @brief 關閉鬧鐘觸發 overlay
 */
void module_ui_alarm_trigger_close(void);
