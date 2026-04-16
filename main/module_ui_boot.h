#pragma once

/**
 * @brief 建立並顯示開機畫面（非阻塞）
 *
 * 須在 module_lvgl_init() 之後呼叫
 * 返回後可透過 module_ui_boot_set_progress() 更新進度
 */
void module_ui_boot_show(void);

/**
 * @brief 更新開機畫面的進度條數值
 *
 * @param value 進度值（0–100）
 */
void module_ui_boot_set_progress(int value);
