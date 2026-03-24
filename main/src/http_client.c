#include "http_client.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha_config.h"

static const char* TAG = "http_client";

static char s_base_url[128];
static char s_token[256];
static bool s_use_https;
static uint16_t s_port;
static uint32_t s_timeout_ms;
static bool s_verify_ssl;
static uint8_t s_max_retries;
static const char* s_user_agent;

void http_client_init(const char* base_url, const char* token, bool use_https)
{
    strncpy(s_base_url, base_url, sizeof(s_base_url) - 1);
    s_base_url[sizeof(s_base_url) - 1] = '\0';
    const char* p = strstr(base_url, "://");
    p = p ? p + 3 : base_url;
    // Use default port if not in URL, but allow g_ha_config to override if available
    s_port = strchr(p, ':') ? 0 : (g_ha_config.port ? g_ha_config.port : HA_DEFAULT_PORT);
    s_timeout_ms = g_ha_config.timeout_ms ? g_ha_config.timeout_ms : HA_DEFAULT_TIMEOUT_MS;
    s_verify_ssl = g_ha_config.verify_ssl;
    s_max_retries = g_ha_config.max_retries ? g_ha_config.max_retries : HA_DEFAULT_MAX_RETRIES;
    s_user_agent = g_ha_config.user_agent ? g_ha_config.user_agent : HA_DEFAULT_USER_AGENT;
    strncpy(s_token, token, sizeof(s_token) - 1);
    s_token[sizeof(s_token) - 1] = '\0';
    s_use_https = use_https;
}

const char* http_client_get_base_url(void) { return s_base_url; }

const char* http_client_get_token(void) { return s_token; }

bool http_client_is_configured(void) { return s_base_url[0] != '\0'; }

static esp_http_client_handle_t open_client(const char* url)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = s_timeout_ms,
        .transport_type = s_use_https ? HTTP_TRANSPORT_OVER_SSL : HTTP_TRANSPORT_OVER_TCP,
    };

    if (s_use_https)
    {
        if (s_verify_ssl)
        {
            cfg.crt_bundle_attach = esp_crt_bundle_attach;
        }
        else
        {
            cfg.skip_cert_common_name_check = true;
        }
    }

    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    if (cli == NULL)
    {
        ESP_LOGE(TAG, "http client init failed");
        return NULL;
    }

    if (s_token[0] != '\0')
    {
        char header[sizeof(s_token) + sizeof(HA_HTTP_BEARER_PREFIX)];
        snprintf(header, sizeof(header), "%s%s", HA_HTTP_BEARER_PREFIX, s_token);
        esp_http_client_set_header(cli, "Authorization", header);
    }
    esp_http_client_set_header(cli, "Content-Type", "application/json");
    esp_http_client_set_header(cli, HA_HTTP_HEADER_ACCEPT, HA_HTTP_CONTENT_JSON);
    if (s_user_agent && s_user_agent[0] != '\0')
    {
        esp_http_client_set_header(cli, HA_HTTP_HEADER_USER_AGENT, s_user_agent);
    }
    return cli;
}

bool http_client_get(const char* path, char** out, size_t* out_len)
{
    if (!out)
    {
        return false;
    }
    *out = NULL;
    if (out_len)
    {
        *out_len = 0;
    }
    if (!http_client_is_configured())
    {
        ESP_LOGE(TAG, "http_client not configured");
        return false;
    }
    char url[256];
    if (s_port)
    {
        snprintf(url, sizeof(url), "%s:%u%s", s_base_url, s_port, path);
    }
    else
    {
        snprintf(url, sizeof(url), "%s%s", s_base_url, path);
    }

    int max_retries = s_max_retries ? s_max_retries : HA_DEFAULT_MAX_RETRIES;
    for (int attempt = 1; attempt <= max_retries; ++attempt)
    {
        esp_http_client_handle_t cli = open_client(url);
        if (cli == NULL)
        {
            if (attempt < max_retries)
            {
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            return false;
        }

        esp_err_t err = esp_http_client_open(cli, 0);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "HTTP GET failed: %s (attempt %d)", esp_err_to_name(err), attempt);
            esp_http_client_cleanup(cli);
        }
        else
        {
            int status = esp_http_client_fetch_headers(cli);
            if (status >= 0)
            {
                int http_status = esp_http_client_get_status_code(cli);
                if (http_status == 200)
                {
                    ssize_t content_length = esp_http_client_get_content_length(cli);
                    size_t capacity = content_length > 0 ? (size_t)content_length + 1 : 1024;
                    if (capacity < 1024)
                    {
                        capacity = 1024;
                    }
                    char* buffer = (char*)malloc(capacity);
                    if (!buffer)
                    {
                        ESP_LOGE(TAG, "Failed to allocate response buffer");
                        esp_http_client_cleanup(cli);
                        return false;
                    }
                    size_t total_read = 0;
                    bool read_error = false;
                    while (true)
                    {
                        if (total_read >= capacity - 1)
                        {
                            size_t new_capacity = capacity * 2;
                            char* new_buffer = (char*)realloc(buffer, new_capacity);
                            if (!new_buffer)
                            {
                                ESP_LOGE(TAG, "Failed to grow response buffer");
                                free(buffer);
                                esp_http_client_cleanup(cli);
                                return false;
                            }
                            buffer = new_buffer;
                            capacity = new_capacity;
                        }
                        int bytes = esp_http_client_read(cli, buffer + total_read,
                                                         capacity - 1 - total_read);
                        if (bytes < 0)
                        {
                            ESP_LOGE(TAG, "HTTP read error on %s", url);
                            read_error = true;
                            break;
                        }
                        if (bytes == 0)
                        {
                            break;
                        }
                        total_read += bytes;
                    }
                    if (read_error)
                    {
                        free(buffer);
                        esp_http_client_cleanup(cli);
                    }
                    else
                    {
                        buffer[total_read] = '\0';
                        if (out_len)
                        {
                            *out_len = total_read;
                        }
                        *out = buffer;
                        esp_http_client_cleanup(cli);
                        return true;
                    }
                }
                else
                {
                    ESP_LOGW(TAG, "GET %s returned status %d", url, http_status);
                    esp_http_client_cleanup(cli);
                }
            }
            else
            {
                ESP_LOGE(TAG, "HTTP status error: %d", status);
                esp_http_client_cleanup(cli);
            }
        }
        if (attempt < max_retries)
        {
            ESP_LOGW(TAG, "retrying GET %s (%d/%d)", url, attempt + 1, max_retries);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
    return false;
}

bool http_client_post(const char* path, const char* payload)
{
    ESP_LOGI(TAG, ">>> http_client_post: path='%s'", path ? path : "NULL");
    ESP_LOGI(TAG, "    payload='%s'", payload ? payload : "NULL");

    if (!http_client_is_configured())
    {
        ESP_LOGE(TAG, "http_client not configured (base_url='%s', token='%s')",
                 s_base_url[0] ? s_base_url : "EMPTY", s_token[0] ? "SET" : "EMPTY");
        return false;
    }
    char url[256];
    if (s_port)
    {
        snprintf(url, sizeof(url), "%s:%u%s", s_base_url, s_port, path);
    }
    else
    {
        snprintf(url, sizeof(url), "%s%s", s_base_url, path);
    }

    ESP_LOGI(TAG, "    full URL='%s'", url);

    int max_retries = s_max_retries ? s_max_retries : HA_DEFAULT_MAX_RETRIES;
    for (int attempt = 1; attempt <= max_retries; ++attempt)
    {
        ESP_LOGI(TAG, "    Attempt %d/%d", attempt, max_retries);
        esp_http_client_handle_t cli = open_client(url);
        if (cli == NULL)
        {
            ESP_LOGE(TAG, "    Failed to open HTTP client");
            if (attempt < max_retries)
            {
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            return false;
        }

        esp_http_client_set_method(cli, HTTP_METHOD_POST);
        esp_http_client_set_post_field(cli, payload, strlen(payload));

        ESP_LOGI(TAG, "    Performing HTTP POST...");
        esp_err_t err = esp_http_client_perform(cli);
        int http_status = esp_http_client_get_status_code(cli);

        ESP_LOGI(TAG, "    HTTP result: err=%s, status=%d", esp_err_to_name(err), http_status);

        if (err == ESP_OK && http_status < 300)
        {
            ESP_LOGI(TAG, "<<< http_client_post: SUCCESS");
            esp_http_client_cleanup(cli);
            return true;
        }

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "HTTP POST failed: %s (attempt %d)", esp_err_to_name(err), attempt);
        }
        else
        {
            ESP_LOGW(TAG, "POST %s returned status %d", url, http_status);
        }
        esp_http_client_cleanup(cli);
        if (attempt < max_retries)
        {
            ESP_LOGW(TAG, "retrying POST %s (%d/%d)", url, attempt + 1, max_retries);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
    ESP_LOGE(TAG, "<<< http_client_post: FAILED after %d attempts", max_retries);
    return false;
}

bool http_client_put(const char* path, const char* payload)
{
    if (!http_client_is_configured())
    {
        ESP_LOGE(TAG, "http_client not configured");
        return false;
    }
    char url[256];
    if (s_port)
    {
        snprintf(url, sizeof(url), "%s:%u%s", s_base_url, s_port, path);
    }
    else
    {
        snprintf(url, sizeof(url), "%s%s", s_base_url, path);
    }

    int max_retries = s_max_retries ? s_max_retries : HA_DEFAULT_MAX_RETRIES;
    for (int attempt = 1; attempt <= max_retries; ++attempt)
    {
        esp_http_client_handle_t cli = open_client(url);
        if (cli == NULL)
        {
            if (attempt < max_retries)
            {
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            return false;
        }

        esp_http_client_set_method(cli, HTTP_METHOD_PUT);
        esp_http_client_set_post_field(cli, payload, strlen(payload));

        esp_err_t err = esp_http_client_perform(cli);
        int http_status = esp_http_client_get_status_code(cli);
        if (err == ESP_OK && http_status < 300)
        {
            esp_http_client_cleanup(cli);
            return true;
        }

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "HTTP PUT failed: %s (attempt %d)", esp_err_to_name(err), attempt);
        }
        else
        {
            ESP_LOGW(TAG, "PUT %s returned status %d", url, http_status);
        }
        esp_http_client_cleanup(cli);
        if (attempt < max_retries)
        {
            ESP_LOGW(TAG, "retrying PUT %s (%d/%d)", url, attempt + 1, max_retries);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
    return false;
}
