#pragma once

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
