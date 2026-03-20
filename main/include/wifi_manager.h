#pragma once

#include "esp_err.h"
#include "esp_wifi_types.h"

typedef enum
{
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED
} wifi_connection_state_t;

typedef struct
{
    wifi_connection_state_t state;
    uint8_t retry_count;
    int8_t rssi;
    uint32_t ip_addr;
} wifi_status_t;

// Callback type for WiFi events
// Provides the current WiFi status whenever a state change occurs
typedef void (*wifi_event_cb_t)(const wifi_status_t* status, void* user_data);

// Function declarations
esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_start(void);
esp_err_t wifi_manager_connect(const char* ssid, const char* pass);
wifi_status_t wifi_manager_get_status(void);
void wifi_manager_register_callback(wifi_event_cb_t callback, void* user_data);
void wifi_manager_unregister_callback(wifi_event_cb_t callback);
esp_err_t wifi_manager_stop(void);
esp_err_t wifi_manager_deinit(void);
