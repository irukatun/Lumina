#pragma once

/**
 * @brief 建立開機畫面並播放進場動畫（阻塞）
 *
 * 須在 module_lvgl_init() 之後呼叫
 * 包含背光淡入與 UI 淡入，完成後返回
 * 返回後可透過 set_progress / set_status 更新畫面
 */
void module_ui_boot_show(void);

/**
 * @brief 更新開機畫面的進度條數值
 *
 * @param value 進度值（0–100）
 */
void module_ui_boot_set_progress(int value);

/**
 * @brief 更新進度條上方的狀態文字
 *
 * @param text 顯示字串，例如 "正在初始化 IMU..."
 */
void module_ui_boot_set_status(const char *text);

/**
 * @brief 完成開機：停止旋轉動畫，弧線填滿並顯示打勾圖標（非阻塞）
 *
 * 動畫約 500ms，呼叫後請自行 vTaskDelay 等待動畫播完再切換畫面
 */
void module_ui_boot_set_complete(void);
