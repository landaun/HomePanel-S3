#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "lvgl.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "ha_client.h"
#include "cJSON.h"
#include "lcd_rgb_port.h"
#include "driver/uart.h"
#include "ui_style.h"
#include "ui_icons.h"
#include "config.h"
#include "offline_tracker.h"
#include "command_queue.h"
#include "screen_manager.h"
#include "discovery.h"
#include "tabbed_ui.h"

/*-------------------------------------------------
 * Function declarations
 *------------------------------------------------*/
static void ha_init_state_task(void* param);
static void ha_discovery_task(void* param);
static void ha_poll_task(void* param);
static void start_background_tasks(void);
static void panel_start_task(void* param);
/*-------------------------------------------------
 * LVGL UI helpers
 *------------------------------------------------*/
static const char* PANEL_TAG = "panel";

/*-------------------------------------------------
 * Globals & constants
 *------------------------------------------------*/
#define HA_POLL_STACK_SIZE 8192
#define HA_POLL_PRIORITY 3

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static panel_config_t g_cfg;

// Discovery result — populated by ha_discovery_task, read by ha_poll_task
disc_result_t g_discovery = {0};
SemaphoreHandle_t g_discovery_mutex = NULL;

#ifndef CONFIG_HA_OFFLINE_THRESHOLD
#define CONFIG_HA_OFFLINE_THRESHOLD 3
#endif

/*-------------------------------------------------
 * Wi‑Fi Event Handler
 *------------------------------------------------*/
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
                               void* event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
        {
            ESP_LOGI(PANEL_TAG, "Wi‑Fi started, connecting…");
            update_status_indicators(false, false);
            esp_err_t err = esp_wifi_connect();
            if (err != ESP_OK)
            {
                ESP_LOGE(PANEL_TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
            }
            break;
        }
        case WIFI_EVENT_STA_DISCONNECTED:
        {
            ESP_LOGW(PANEL_TAG, "Disconnected, retrying…");
            update_status_indicators(false, false);
            esp_err_t err = esp_wifi_connect();
            if (err != ESP_OK)
            {
                ESP_LOGE(PANEL_TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
            }
            break;
        }
        default:
            break;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(PANEL_TAG, "Got IP: " IPSTR ", network ready", IP2STR(&event->ip_info.ip));
        ESP_LOGI(PANEL_TAG, "Gateway: " IPSTR, IP2STR(&event->ip_info.gw));
        ESP_LOGI(PANEL_TAG, "Netmask: " IPSTR, IP2STR(&event->ip_info.netmask));

        update_status_indicators(true, false);
        offline_tracker_reset();
        tabbed_ui_set_offline(false);
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/*-------------------------------------------------
 * Task and timer startup
 *------------------------------------------------*/
static void start_background_tasks(void)
{
    // Create discovery mutex before launching tasks that use g_discovery
    if (!g_discovery_mutex)
        g_discovery_mutex = xSemaphoreCreateMutex();

    xTaskCreatePinnedToCore(ha_init_state_task, "ha_init", 4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(ha_discovery_task, "ha_disc", 12288, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(ha_poll_task, "ha_poll", HA_POLL_STACK_SIZE, NULL, HA_POLL_PRIORITY,
                            NULL, 1);
}

/*-------------------------------------------------
 * Entity helpers
 *------------------------------------------------*/

static cJSON* find_entity_state(cJSON* states, const char* entity_id)
{
    if (!cJSON_IsArray(states) || !entity_id || entity_id[0] == '\0')
    {
        return NULL;
    }
    cJSON* item = NULL;
    cJSON_ArrayForEach(item, states)
    {
        cJSON* id = cJSON_GetObjectItem(item, "entity_id");
        if (cJSON_IsString(id) && strcmp(id->valuestring, entity_id) == 0)
        {
            return item;
        }
    }
    return NULL;
}

static void ha_init_state_task(void* param)
{
    (void)param;
    // Initial state is now populated by ha_poll_task after discovery completes.
    vTaskDelete(NULL);
}

/*-------------------------------------------------
 * Discovery task — populates g_discovery from HA
 *------------------------------------------------*/
static void ha_discovery_task(void* param)
{
    (void)param;
    // Give LVGL time to finish initial draw
    vTaskDelay(pdMS_TO_TICKS(500));

#ifndef CONFIG_HA_FILTER_AREA
#define CONFIG_HA_FILTER_AREA ""
#endif
    const char* area_filter = CONFIG_HA_FILTER_AREA;

    // Retry discovery until we get entities (WiFi may not be up yet on first attempt)
    disc_result_t fresh = {0};
    for (;;)
    {
        esp_err_t err = discovery_run(area_filter, &fresh);
        if (err == ESP_OK && fresh.count > 0)
            break;

        ESP_LOGW(PANEL_TAG, "Discovery returned 0 entities or failed (%s), retrying in 5s...",
                 esp_err_to_name(err));
        discovery_free(&fresh);
        memset(&fresh, 0, sizeof(fresh));
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    // Save to NVS cache for fast boot next time
    discovery_save_cache(&fresh);

    // Hand off to global (protected by mutex)
    if (xSemaphoreTake(g_discovery_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        discovery_free(&g_discovery);
        g_discovery = fresh;
        xSemaphoreGive(g_discovery_mutex);
    }
    else
    {
        ESP_LOGW(PANEL_TAG, "Could not acquire discovery mutex — keeping existing result");
        discovery_free(&fresh);
    }

    // Build (or rebuild) the tabbed UI from discovery results
    if (tabbed_ui_ready())
        tabbed_ui_rebuild(&g_discovery);
    else
        tabbed_ui_create(&g_discovery);

    vTaskDelete(NULL);
}

/*-------------------------------------------------
 * Background polling task
 *------------------------------------------------*/
static void ha_poll_task(void* param)
{
    (void)param;
    offline_tracker_init(CONFIG_HA_OFFLINE_THRESHOLD);

    for (;;)
    {
        bool api_ok = false;

        // Skip polling until the tabbed UI is ready
        if (!tabbed_ui_ready())
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        cJSON* states = NULL;
        if (ha_client_get_states(&states) == ESP_OK && cJSON_IsArray(states))
        {
            api_ok = true;
        }

        // Iterate over discovered entities and update the tabbed UI
        if (xSemaphoreTake(g_discovery_mutex, pdMS_TO_TICKS(500)) == pdTRUE)
        {
            // Per-domain slot counters
            size_t slot[DISC_DOMAIN_COUNT] = {0};

            for (size_t i = 0; i < g_discovery.count; i++)
            {
                const disc_entity_t* e = &g_discovery.entities[i];
                size_t s = slot[e->domain]++;

                if (!api_ok || !states)
                    continue;

                cJSON* es = find_entity_state(states, e->entity_id);
                if (!es) continue;

                cJSON* state_val = cJSON_GetObjectItem(es, "state");
                if (!cJSON_IsString(state_val)) continue;
                const char* sv = state_val->valuestring;
                cJSON* attr = cJSON_GetObjectItem(es, "attributes");

                switch (e->domain)
                {
                    case DISC_DOMAIN_LIGHT:
                    {
                        bool on = strcmp(sv, "on") == 0;
                        uint8_t brightness = 0;
                        if (cJSON_IsObject(attr))
                        {
                            cJSON* b = cJSON_GetObjectItem(attr, "brightness");
                            if (cJSON_IsNumber(b) && b->valueint >= 0)
                                brightness = (uint8_t)b->valueint;
                        }
                        tabbed_ui_update_light(s, on, brightness);
                        break;
                    }
                    case DISC_DOMAIN_TEMPERATURE:
                    {
                        float temp = strtof(sv, NULL);
                        tabbed_ui_update_temperature(s, temp);
                        break;
                    }
                    case DISC_DOMAIN_CLIMATE:
                    {
                        float temp = 0.0f;
                        float target = 0.0f;
                        if (cJSON_IsObject(attr))
                        {
                            cJSON* ct = cJSON_GetObjectItem(attr, "current_temperature");
                            if (ct && cJSON_IsNumber(ct))
                                temp = (float)ct->valuedouble;
                            cJSON* t = cJSON_GetObjectItem(attr, "temperature");
                            if (!t || !cJSON_IsNumber(t))
                                t = cJSON_GetObjectItem(attr, "target_temp");
                            if (t && cJSON_IsNumber(t))
                                target = (float)t->valuedouble;
                        }
                        tabbed_ui_update_temperature_with_target(s, temp, target);
                        break;
                    }
                    case DISC_DOMAIN_OCCUPANCY:
                    {
                        bool occupied = strcmp(sv, "on") == 0;
                        tabbed_ui_update_occupancy(s, occupied);
                        break;
                    }
                    case DISC_DOMAIN_MEDIA:
                    {
                        float volume = -1.0f; // negative = unknown
                        const char* media_title = "";
                        if (cJSON_IsObject(attr))
                        {
                            cJSON* v = cJSON_GetObjectItem(attr, "volume_level");
                            if (cJSON_IsNumber(v))
                                volume = (float)v->valuedouble;
                            cJSON* t = cJSON_GetObjectItem(attr, "media_title");
                            if (!t) t = cJSON_GetObjectItem(attr, "media_content_id");
                            if (cJSON_IsString(t) && t->valuestring)
                                media_title = t->valuestring;
                        }
                        tabbed_ui_update_media(s, sv, volume, media_title);
                        break;
                    }
                    default:
                        break;
                }
            }

            xSemaphoreGive(g_discovery_mutex);
        }

        if (states)
            cJSON_Delete(states);

        update_status_indicators(true, api_ok);
        bool offline = offline_tracker_update(api_ok);
        tabbed_ui_set_offline(offline);

        vTaskDelay(pdMS_TO_TICKS(10000)); /* refresh every 10 s */
    }
}

/*-------------------------------------------------
 * Wi‑Fi setup helper
 *------------------------------------------------*/
__attribute__((unused)) static void wifi_init(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_cfg = {0};
    strncpy((char*)wifi_cfg.sta.ssid, g_cfg.wifi_ssid, sizeof(wifi_cfg.sta.ssid));
    strncpy((char*)wifi_cfg.sta.password, g_cfg.wifi_password, sizeof(wifi_cfg.sta.password));
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
}

/*-------------------------------------------------
 * app_main
 *------------------------------------------------*/
static void panel_start_task(void* param)
{
    (void)param;

    // Initialize NVS
    ESP_ERROR_CHECK(config_init());
    ESP_ERROR_CHECK(config_load(&g_cfg));
    ESP_ERROR_CHECK(ha_client_setup());

    // Initialize command queue for HA commands
    ESP_LOGI(PANEL_TAG, "Initializing command queue...");
    if (!command_queue_init())
    {
        ESP_LOGE(PANEL_TAG, "Failed to initialize command queue");
        abort();
    }
    ESP_LOGI(PANEL_TAG, "Command queue initialized successfully");

    // Initialize TCP/IP stack and default event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create Wi-Fi event group
    wifi_event_group = xEventGroupCreate();

    // Initialize LCD, LVGL, and Touch first
    ESP_LOGI(PANEL_TAG, "Install RGB LCD panel driver");
    lcd_rgb_init();

    // Initialize UI styles
    ui_style_init();
    ui_icons_init();

    // Draw a minimal splash screen — tabbed_ui_create() replaces this once discovery completes
    {
        lv_obj_t* scr = lv_scr_act();
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x121212), 0);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
        lv_obj_t* lbl = lv_label_create(scr);
        lv_obj_add_style(lbl, ui_style_value_secondary(), 0);
        lv_label_set_text(lbl, "Connecting...");
        lv_obj_center(lbl);
    }

    // Initialize screen timeout and double-tap wake/sleep
    screen_manager_init();

    start_background_tasks();

    // Initialize Wi-Fi (station mode)
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register Wi-Fi and IP event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta =
            {
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            },
    };
    strncpy((char*)wifi_config.sta.ssid, g_cfg.wifi_ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, g_cfg.wifi_password, sizeof(wifi_config.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for Wi-Fi connection (IP address)
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    // Test network connectivity to Home Assistant
    ESP_LOGI(PANEL_TAG, "Testing connectivity to Home Assistant at %s...", g_cfg.ha_url);

    // Extract IP from URL (simple parse for http://IP:PORT format)
    char test_ip[32] = {0};
    const char* url_start = strstr(g_cfg.ha_url, "://");
    if (url_start) {
        url_start += 3; // Skip "://"
        const char* port_start = strchr(url_start, ':');
        if (port_start) {
            size_t ip_len = port_start - url_start;
            if (ip_len < sizeof(test_ip)) {
                strncpy(test_ip, url_start, ip_len);
                ESP_LOGI(PANEL_TAG, "Extracted HA IP: %s", test_ip);
            }
        }
    }

    ESP_LOGI(PANEL_TAG, "Panel ready!");
    vTaskDelete(NULL);
}

void app_main(void)
{
    const BaseType_t created =
        xTaskCreatePinnedToCore(panel_start_task, "panel_main", 8192, NULL, 5, NULL, 1);
    if (created != pdPASS)
    {
        ESP_LOGE(PANEL_TAG, "Failed to create panel_main task");
        abort();
    }
    vTaskDelete(NULL);
}
