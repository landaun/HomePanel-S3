#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "esp_netif.h"
#include <string.h>

static const char* TAG = "wifi_manager";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define MAXIMUM_RETRY 5

static EventGroupHandle_t s_wifi_event_group;
static wifi_status_t s_wifi_status = {
    .state = WIFI_STATE_DISCONNECTED, .retry_count = 0, .rssi = 0, .ip_addr = 0};

#define MAX_WIFI_EVENT_CB 5
typedef struct
{
    wifi_event_cb_t cb;
    void* arg;
} wifi_cb_entry_t;

static wifi_cb_entry_t s_callbacks[MAX_WIFI_EVENT_CB];
static size_t s_callback_count = 0;

static TimerHandle_t s_retry_timer = NULL;
static void retry_timer_cb(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "Retrying connection");
    esp_wifi_connect();
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
                          void* event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            s_wifi_status.state = WIFI_STATE_CONNECTING;
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "Connected to AP");
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
        {
            wifi_event_sta_disconnected_t* disconn = (wifi_event_sta_disconnected_t*)event_data;
            ESP_LOGW(TAG, "Disconnected from AP, reason=%d", disconn->reason);
            if (s_wifi_status.retry_count < MAXIMUM_RETRY)
            {
                uint32_t delay_ms = (1 << s_wifi_status.retry_count) * 1000;
                if (s_retry_timer)
                {
                    xTimerStop(s_retry_timer, 0);
                    xTimerChangePeriod(s_retry_timer, pdMS_TO_TICKS(delay_ms), 0);
                    xTimerStart(s_retry_timer, 0);
                }
                else
                {
                    esp_wifi_connect();
                }
                s_wifi_status.retry_count++;
                ESP_LOGI(TAG, "Retry to connect to the AP (%u)", s_wifi_status.retry_count);
            }
            else
            {
                ESP_LOGE(TAG, "Failed to connect after %d retries", MAXIMUM_RETRY);
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                s_wifi_status.state = WIFI_STATE_FAILED;
            }
            break;
        }

        default:
            break;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        char ip_str[IP4ADDR_STRLEN_MAX];
        esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, sizeof(ip_str));
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
        {
            s_wifi_status.rssi = ap_info.rssi;
        }
        s_wifi_status.ip_addr = event->ip_info.ip.addr;
        s_wifi_status.retry_count = 0;
        s_wifi_status.state = WIFI_STATE_CONNECTED;
        ESP_LOGI(TAG, "Got IP: %s, RSSI: %d", ip_str, s_wifi_status.rssi);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }

    // Notify callbacks if registered
    for (size_t i = 0; i < s_callback_count; ++i)
    {
        if (s_callbacks[i].cb)
        {
            s_callbacks[i].cb(&s_wifi_status, s_callbacks[i].arg);
        }
    }
}

esp_err_t wifi_manager_init(void)
{
    esp_err_t ret = ESP_OK;

    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group)
    {
        return ESP_ERR_NO_MEM;
    }
    s_retry_timer = xTimerCreate("wifi_retry", pdMS_TO_TICKS(1000), pdFALSE, NULL, retry_timer_cb);
    if (!s_retry_timer)
    {
        return ESP_ERR_NO_MEM;
    }

    ret = esp_netif_init();
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK)
    {
        return ret;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL,
                                              NULL);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL,
                                              NULL);
    return ret;
}

esp_err_t wifi_manager_connect(const char* ssid, const char* pass)
{
    wifi_config_t wifi_config = {
        .sta =
            {
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                .pmf_cfg = {.capable = true, .required = false},
            },
    };

    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));

    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK)
        return ret;

    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK)
        return ret;

    return esp_wifi_start();
}

esp_err_t wifi_manager_start(void) { return esp_wifi_start(); }

wifi_status_t wifi_manager_get_status(void)
{
    // Update RSSI if connected
    if (s_wifi_status.state == WIFI_STATE_CONNECTED)
    {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
        {
            s_wifi_status.rssi = ap_info.rssi;
        }
    }
    return s_wifi_status;
}

void wifi_manager_register_callback(wifi_event_cb_t callback, void* user_data)
{
    if (s_callback_count < MAX_WIFI_EVENT_CB)
    {
        s_callbacks[s_callback_count].cb = callback;
        s_callbacks[s_callback_count].arg = user_data;
        s_callback_count++;
    }
}

void wifi_manager_unregister_callback(wifi_event_cb_t callback)
{
    for (size_t i = 0; i < s_callback_count; ++i)
    {
        if (s_callbacks[i].cb == callback)
        {
            s_callbacks[i] = s_callbacks[s_callback_count - 1];
            s_callback_count--;
            break;
        }
    }
}

esp_err_t wifi_manager_stop(void)
{
    if (s_retry_timer)
    {
        xTimerStop(s_retry_timer, 0);
    }
    esp_err_t ret = esp_wifi_stop();
    if (ret == ESP_OK)
    {
        s_wifi_status.state = WIFI_STATE_DISCONNECTED;
        s_wifi_status.retry_count = 0;
    }
    return ret;
}

esp_err_t wifi_manager_deinit(void)
{
    esp_err_t ret = wifi_manager_stop();
    if (ret != ESP_OK)
        return ret;

    ret = esp_wifi_deinit();
    if (ret != ESP_OK)
        return ret;

    if (s_retry_timer)
    {
        xTimerDelete(s_retry_timer, 0);
        s_retry_timer = NULL;
    }
    if (s_wifi_event_group)
    {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }

    return ESP_OK;
}
