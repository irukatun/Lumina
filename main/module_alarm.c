// C 標準函式庫
#include <string.h>
#include <stdio.h>

// ESP-IDF 系統組件
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// 本專案模組
#include "module_alarm.h"
#include "module_nvs.h"
#include "module_time.h"

// 日誌標籤
static const char *TAG = "M_Alarm";

// ======================================================================
// 私有巨集
// ======================================================================

#define NVS_NS          "alarm"
#define SLOT_DELETED    0xFF    // enabled 欄位的哨兵值，表示 slot 空位
#define TASK_STACK      3072
#define TASK_PRIORITY   3
#define MAX_CALLBACKS   ALARM_MAX_COUNT

// ======================================================================
// 私有變數
// ======================================================================

static alarm_t           s_alarms[ALARM_MAX_COUNT];
static bool              s_slot_used[ALARM_MAX_COUNT];
static SemaphoreHandle_t s_mutex         = NULL;
static alarm_trigger_cb_t s_cbs[MAX_CALLBACKS];

// ======================================================================
// 私有函式前向宣告
// ======================================================================

static void    key_h(int slot, char *buf);
static void    key_m(int slot, char *buf);
static void    key_r(int slot, char *buf);
static void    key_e(int slot, char *buf);
static void    nvs_save_slot(int slot);
static void    fire_callbacks(int slot, const alarm_t *a);
static void    check_and_fire(const module_time_t *t);
static void    alarm_task(void *arg);

// ======================================================================
// 私有函式實作
// ======================================================================

static void key_h(int slot, char *buf) { snprintf(buf, 4, "%dh", slot); }
static void key_m(int slot, char *buf) { snprintf(buf, 4, "%dm", slot); }
static void key_r(int slot, char *buf) { snprintf(buf, 4, "%dr", slot); }
static void key_e(int slot, char *buf) { snprintf(buf, 4, "%de", slot); }

// 防禦性寫入：先寫 SLOT_DELETED，再寫 h/m/r，最後寫 enabled
// 中途掉電只會看到 SLOT_DELETED（空 slot），不會讀到半殘資料
static void nvs_save_slot(int slot)
{
    char k[4];
    key_e(slot, k); module_nvs_set_u8(NVS_NS, k, SLOT_DELETED);

    if (!s_slot_used[slot]) return;  // 已刪除，只需寫哨兵

    key_h(slot, k); module_nvs_set_u8(NVS_NS, k, s_alarms[slot].hour);
    key_m(slot, k); module_nvs_set_u8(NVS_NS, k, s_alarms[slot].minute);
    key_r(slot, k); module_nvs_set_u8(NVS_NS, k, s_alarms[slot].repeat_days);
    key_e(slot, k); module_nvs_set_u8(NVS_NS, k, s_alarms[slot].enabled);
}

static void fire_callbacks(int slot, const alarm_t *a)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (s_cbs[i]) s_cbs[i](slot, a);
    }
}

static void check_and_fire(const module_time_t *t)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    for (int i = 0; i < ALARM_MAX_COUNT; i++) {
        if (!s_slot_used[i] || !s_alarms[i].enabled) continue;
        if (s_alarms[i].hour != (uint8_t)t->hour) continue;
        if (s_alarms[i].minute != (uint8_t)t->minute) continue;

        // 重複日檢查（repeat_days == 0 表示一次性，不檢查星期）
        if (s_alarms[i].repeat_days != 0) {
            if (!(s_alarms[i].repeat_days & ALARM_WEEKDAY_BIT(t->weekday))) continue;
        }

        bool is_oneshot = (s_alarms[i].repeat_days == 0);
        if (is_oneshot) {
            s_alarms[i].enabled = 0;
        }

        alarm_t snapshot = s_alarms[i];
        xSemaphoreGive(s_mutex);

        if (is_oneshot) {
            char k[4];
            key_e(i, k);
            module_nvs_set_u8(NVS_NS, k, 0);
        }

        ESP_LOGI(TAG, "鬧鐘觸發 slot %d (%02d:%02d)", i, snapshot.hour, snapshot.minute);
        fire_callbacks(i, &snapshot);

        xSemaphoreTake(s_mutex, portMAX_DELAY);
    }

    xSemaphoreGive(s_mutex);
}

static void alarm_task(void *arg)
{
    while (1) {
        module_time_t t = {0};
        if (module_time_get(&t) == ESP_OK &&
            module_time_get_status() == MODULE_TIME_STATUS_OK) {
            check_and_fire(&t);
        }

        // 休眠到下一分鐘整點，自我修正不漂移
        uint32_t sleep_ms = (uint32_t)(60 - t.second) * 1000;
        if (sleep_ms == 0 || sleep_ms > 60000) sleep_ms = 60000;
        vTaskDelay(pdMS_TO_TICKS(sleep_ms));
    }
}

// ======================================================================
// 公開 API
// ======================================================================

esp_err_t module_alarm_init(void)
{
    memset(s_alarms,    0, sizeof(s_alarms));
    memset(s_slot_used, 0, sizeof(s_slot_used));
    memset(s_cbs,       0, sizeof(s_cbs));

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "mutex 建立失敗");
        return ESP_ERR_NO_MEM;
    }

    // 從 NVS 載入所有 slot
    char k[4];
    for (int i = 0; i < ALARM_MAX_COUNT; i++) {
        uint8_t e;
        key_e(i, k);
        if (module_nvs_get_u8(NVS_NS, k, &e) != ESP_OK) continue;
        if (e == SLOT_DELETED) continue;

        uint8_t h = 0, m = 0, r = 0;
        key_h(i, k); module_nvs_get_u8(NVS_NS, k, &h);
        key_m(i, k); module_nvs_get_u8(NVS_NS, k, &m);
        key_r(i, k); module_nvs_get_u8(NVS_NS, k, &r);

        s_alarms[i].hour        = h;
        s_alarms[i].minute      = m;
        s_alarms[i].repeat_days = r;
        s_alarms[i].enabled     = e;
        s_slot_used[i]          = true;
    }

    xTaskCreate(alarm_task, "alarm_task", TASK_STACK, NULL, TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "初始化完成，已載入 %d 個鬧鐘", module_alarm_count());
    return ESP_OK;
}

int module_alarm_count(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int cnt = 0;
    for (int i = 0; i < ALARM_MAX_COUNT; i++) {
        if (s_slot_used[i]) cnt++;
    }
    xSemaphoreGive(s_mutex);
    return cnt;
}

bool module_alarm_get(int slot, alarm_t *out)
{
    if (slot < 0 || slot >= ALARM_MAX_COUNT) return false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool used = s_slot_used[slot];
    if (used) *out = s_alarms[slot];
    xSemaphoreGive(s_mutex);
    return used;
}

int module_alarm_add(const alarm_t *alarm)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int found = -1;
    for (int i = 0; i < ALARM_MAX_COUNT; i++) {
        if (!s_slot_used[i]) { found = i; break; }
    }
    if (found >= 0) {
        s_alarms[found]    = *alarm;
        s_slot_used[found] = true;
    }
    xSemaphoreGive(s_mutex);

    if (found >= 0) {
        nvs_save_slot(found);
        ESP_LOGI(TAG, "新增鬧鐘 slot %d (%02d:%02d)", found, alarm->hour, alarm->minute);
    } else {
        ESP_LOGW(TAG, "鬧鐘已滿，無法新增");
    }
    return found;
}

esp_err_t module_alarm_set(int slot, const alarm_t *alarm)
{
    if (slot < 0 || slot >= ALARM_MAX_COUNT) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_alarms[slot]    = *alarm;
    s_slot_used[slot] = true;
    xSemaphoreGive(s_mutex);
    nvs_save_slot(slot);
    return ESP_OK;
}

esp_err_t module_alarm_delete(int slot)
{
    if (slot < 0 || slot >= ALARM_MAX_COUNT) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_slot_used[slot] = false;
    memset(&s_alarms[slot], 0, sizeof(alarm_t));
    xSemaphoreGive(s_mutex);
    nvs_save_slot(slot);
    ESP_LOGI(TAG, "刪除鬧鐘 slot %d", slot);
    return ESP_OK;
}

esp_err_t module_alarm_set_enabled(int slot, bool enabled)
{
    if (slot < 0 || slot >= ALARM_MAX_COUNT) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (!s_slot_used[slot]) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    s_alarms[slot].enabled = enabled ? 1 : 0;
    xSemaphoreGive(s_mutex);
    nvs_save_slot(slot);
    return ESP_OK;
}

bool module_alarm_get_next(alarm_t *out, int *slot_out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < ALARM_MAX_COUNT; i++) {
        if (s_slot_used[i] && s_alarms[i].enabled) {
            if (out)      *out      = s_alarms[i];
            if (slot_out) *slot_out = i;
            xSemaphoreGive(s_mutex);
            return true;
        }
    }
    xSemaphoreGive(s_mutex);
    return false;
}

void module_alarm_add_trigger_cb(alarm_trigger_cb_t cb)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (!s_cbs[i]) { s_cbs[i] = cb; return; }
    }
    ESP_LOGW(TAG, "trigger callback 已達上限");
}

void module_alarm_remove_trigger_cb(alarm_trigger_cb_t cb)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (s_cbs[i] == cb) { s_cbs[i] = NULL; return; }
    }
}
