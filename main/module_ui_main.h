#pragma once

/**
 * @brief 建立主畫面並以淡入動畫切換（非阻塞）
 *
 * 須在 module_ui_boot 流程完成後呼叫。
 * 內部會建立 LVGL timer 每秒更新時鐘，並向 IMU / PIR 註冊 callback。
 */
void module_ui_main_show(void);

/**
 * @brief 銷毀主畫面並釋放所有資源
 *
 * 移除 LVGL timer 及 IMU / PIR callback，刪除螢幕物件。
 * 未來實作多畫面切換時使用。
 */
void module_ui_main_destroy(void);
