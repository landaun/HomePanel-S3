#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"

// Configuration structure as defined in technical specs
typedef struct
{
    char wifi_ssid[32];
    char wifi_password[64];
    char ha_url[128];
    char ha_token[256];
    bool use_https;
    uint16_t update_interval_ms;
} panel_config_t;

// Display configuration
typedef struct
{
    uint8_t brightness;
    uint16_t auto_dim_timeout;
    bool auto_dim_enabled;
    uint8_t dim_level;
} display_config_t;

// Default configuration values
#define DEFAULT_UPDATE_INTERVAL_MS 10000
#define DEFAULT_BRIGHTNESS 100
#define DEFAULT_AUTO_DIM_TIMEOUT 30000
#define DEFAULT_DIM_LEVEL 30

// NVS Storage keys
#define NVS_NAMESPACE "panel_config"
#define NVS_KEY_WIFI_CONFIG "wifi_conf"
#define NVS_KEY_HA_CONFIG "ha_conf"
#define NVS_KEY_DISPLAY_CONFIG "disp_conf"
#define NVS_KEY_TOUCH_CAL "touch_cal"

// Error handling
#define CHECK_ERROR_RETURN(x, msg)                                                                 \
    do                                                                                             \
    {                                                                                              \
        esp_err_t __err = (x);                                                                     \
        if (__err != ESP_OK)                                                                       \
        {                                                                                          \
            ESP_LOGE(TAG, "%s: %s", msg, esp_err_to_name(__err));                                  \
            return __err;                                                                          \
        }                                                                                          \
    } while (0)

// Function declarations
esp_err_t config_init(void);
esp_err_t config_load(panel_config_t* config);
esp_err_t config_save(const panel_config_t* config);
esp_err_t config_reset_to_defaults(void);

// Display configuration functions
esp_err_t config_load_display(display_config_t* config);
esp_err_t config_save_display(const display_config_t* config);
