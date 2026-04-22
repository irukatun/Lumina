// C 標準函式庫
#include <string.h>
#include <stdlib.h>
#include <time.h>

// ESP-IDF 組件
#include <esp_log.h>
#include <esp_err.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>

// 第三方組件（managed_components）
#include <cJSON.h>
#include <zlib.h>

// 本專案模組
#include "secrets.h"
#include "board.h"
#include "module_weather.h"

static const char *TAG = "M_Weather";

// ======================================================================
// 私有巨集
// ======================================================================

#define HTTP_BUF_INITIAL  2048
#define HTTP_BUF_MAX      8192
#define GUNZIP_OUT_SIZE   8192
#define NOW_URL_FMT  "https://%s/v7/weather/now?location=%s&lang=zh-hant"

// ======================================================================
// 私有型別
// ======================================================================

typedef struct {
    uint8_t *buf;
    int      len;
    int      cap;
} http_body_t;

// ======================================================================
// 私有函式前向宣告
// ======================================================================

static esp_err_t weather_http_get(const char *url, http_body_t *body);
static esp_err_t weather_gunzip(const uint8_t *in, int in_len,
                                char **out, int *out_len);
static esp_err_t weather_parse_now(const char *json, module_sync_weather_t *out);

// ======================================================================
// 私有函式實作
// ======================================================================

static esp_err_t weather_http_get(const char *url, http_body_t *body)
{
    esp_http_client_config_t config = {
        .url               = url,
        .timeout_ms        = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "HTTP client 初始化失敗");
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "X-QW-Api-Key", QWEATHER_API_KEY);

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open 失敗: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int content_length = esp_http_client_fetch_headers(client);
    int status_code    = esp_http_client_get_status_code(client);
    ESP_LOGD(TAG, "HTTP %d, content-length: %d", status_code, content_length);

    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP 狀態碼非 200: %d", status_code);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    body->buf = malloc(HTTP_BUF_INITIAL);
    if (body->buf == NULL) {
        ESP_LOGE(TAG, "HTTP buffer malloc 失敗");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }
    body->cap = HTTP_BUF_INITIAL;
    body->len = 0;

    int read_len;
    while ((read_len = esp_http_client_read(client,
                                            (char *)body->buf + body->len,
                                            body->cap - body->len)) > 0) {
        body->len += read_len;
        if (body->len >= body->cap) {
            if (body->cap >= HTTP_BUF_MAX) {
                ESP_LOGW(TAG, "HTTP body 超過上限 %d bytes，截斷", HTTP_BUF_MAX);
                break;
            }
            int new_cap = body->cap * 2;
            if (new_cap > HTTP_BUF_MAX) new_cap = HTTP_BUF_MAX;
            uint8_t *tmp = realloc(body->buf, new_cap);
            if (tmp == NULL) {
                ESP_LOGE(TAG, "HTTP buffer realloc 失敗");
                free(body->buf);
                body->buf = NULL;
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return ESP_ERR_NO_MEM;
            }
            body->buf = tmp;
            body->cap = new_cap;
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (body->len == 0) {
        ESP_LOGW(TAG, "伺服器未回傳資料");
        free(body->buf);
        body->buf = NULL;
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t weather_gunzip(const uint8_t *in, int in_len,
                                char **out, int *out_len)
{
    bool is_gzip = (in_len >= 2 && in[0] == 0x1F && in[1] == 0x8B);

    if (!is_gzip) {
        char *buf = malloc(in_len + 1);
        if (buf == NULL) return ESP_ERR_NO_MEM;
        memcpy(buf, in, in_len);
        buf[in_len] = '\0';
        *out     = buf;
        *out_len = in_len;
        return ESP_OK;
    }

    ESP_LOGD(TAG, "偵測到 gzip，正在解壓...");

    char *output = malloc(GUNZIP_OUT_SIZE + 1);
    if (output == NULL) {
        ESP_LOGE(TAG, "gunzip 輸出 buffer malloc 失敗");
        return ESP_ERR_NO_MEM;
    }

    z_stream strm = {0};
    // MAX_WBITS + 32：自動偵測 gzip / zlib header
    int ret = inflateInit2(&strm, MAX_WBITS + 32);
    if (ret != Z_OK) {
        ESP_LOGE(TAG, "inflateInit2 失敗: %d", ret);
        free(output);
        return ESP_FAIL;
    }

    strm.avail_in  = (uInt)in_len;
    strm.next_in   = (Bytef *)in;
    strm.avail_out = GUNZIP_OUT_SIZE;
    strm.next_out  = (Bytef *)output;

    ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);

    if (ret != Z_STREAM_END) {
        if (strm.avail_out == 0) {
            ESP_LOGE(TAG, "gunzip 緩衝區不足（需要 > %d bytes）", GUNZIP_OUT_SIZE);
        } else {
            ESP_LOGE(TAG, "gunzip 失敗，錯誤碼: %d", ret);
        }
        free(output);
        return ESP_FAIL;
    }

    *out_len         = (int)strm.total_out;
    output[*out_len] = '\0';
    *out             = output;
    ESP_LOGD(TAG, "gunzip 成功: %d → %d bytes", in_len, *out_len);
    return ESP_OK;
}

static esp_err_t weather_parse_now(const char *json, module_sync_weather_t *out)
{
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        ESP_LOGE(TAG, "Weather JSON 解析失敗");
        return ESP_FAIL;
    }

    esp_err_t result = ESP_FAIL;
    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (code == NULL || code->valuestring == NULL ||
        strcmp(code->valuestring, "200") != 0) {
        ESP_LOGE(TAG, "Weather API 回傳非 200 code: %s",
                 (code && code->valuestring) ? code->valuestring : "null");
        goto cleanup;
    }

    cJSON *now = cJSON_GetObjectItem(root, "now");
    if (now == NULL) {
        ESP_LOGE(TAG, "Weather API 回應缺少 now 欄位");
        goto cleanup;
    }

    cJSON *temp       = cJSON_GetObjectItem(now, "temp");
    cJSON *feels_like = cJSON_GetObjectItem(now, "feelsLike");
    cJSON *text       = cJSON_GetObjectItem(now, "text");
    cJSON *humidity   = cJSON_GetObjectItem(now, "humidity");
    cJSON *precip     = cJSON_GetObjectItem(now, "precip");
    cJSON *wind_speed = cJSON_GetObjectItem(now, "windSpeed");

    if (temp == NULL || feels_like == NULL || text == NULL ||
        humidity == NULL || precip == NULL || wind_speed == NULL) {
        ESP_LOGE(TAG, "Weather API 回應缺少必要欄位");
        goto cleanup;
    }

    out->outdoor_temp = (float)atof(temp->valuestring);
    out->feels_like   = (float)atof(feels_like->valuestring);
    snprintf(out->weather_text, sizeof(out->weather_text), "%s", text->valuestring);
    out->humidity   = atoi(humidity->valuestring);
    out->precip     = (float)atof(precip->valuestring);
    out->wind_speed = (float)atof(wind_speed->valuestring);
    result = ESP_OK;

cleanup:
    cJSON_Delete(root);
    return result;
}

// ======================================================================
// 公開 API
// ======================================================================

esp_err_t module_weather_fetch(module_sync_weather_t *out)
{
    char url[128];
    snprintf(url, sizeof(url), NOW_URL_FMT, QWEATHER_API_HOST, CONFIG_WEATHER_LOCATION_ID);

    http_body_t body = {0};
    esp_err_t ret = weather_http_get(url, &body);
    if (ret != ESP_OK) return ret;

    char *json     = NULL;
    int   json_len = 0;
    ret = weather_gunzip(body.buf, body.len, &json, &json_len);
    free(body.buf);
    if (ret != ESP_OK) return ret;

    ret = weather_parse_now(json, out);
    free(json);
    if (ret != ESP_OK) return ret;

    snprintf(out->city, sizeof(out->city), "%s", CONFIG_WEATHER_DEFAULT_CITY);
    out->updated_at = time(NULL);

    ESP_LOGI(TAG, "天氣取得成功: %s %.1f°C(體感%.1f) %s 濕度%d%% 降水%.1fmm 風速%.1fkm/h",
             out->city, out->outdoor_temp, out->feels_like,
             out->weather_text, out->humidity, out->precip, out->wind_speed);
    return ESP_OK;
}
