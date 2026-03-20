#include "config.h"
#include "config_defaults.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdlib.h>

static const char* TAG = "config";

// Default configuration. Credentials start as empty strings and must be
// supplied via environment variables or stored in NVS before use.
static const panel_config_t default_config = {.wifi_ssid = PANEL_DEFAULT_WIFI_SSID,
                                              .wifi_password = PANEL_DEFAULT_WIFI_PASSWORD,
                                              .ha_url = PANEL_DEFAULT_HA_URL,
                                              .ha_token = PANEL_DEFAULT_HA_TOKEN,
                                              .use_https = PANEL_DEFAULT_USE_HTTPS,
                                              .update_interval_ms =
                                                  PANEL_DEFAULT_UPDATE_INTERVAL_MS};

static const display_config_t default_display_config = {.brightness = DEFAULT_BRIGHTNESS,
                                                        .auto_dim_timeout =
                                                            DEFAULT_AUTO_DIM_TIMEOUT,
                                                        .auto_dim_enabled = true,
                                                        .dim_level = DEFAULT_DIM_LEVEL};

static nvs_handle_t storage_handle;

esp_err_t config_init(void)
{
    esp_err_t ret;

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        CHECK_ERROR_RETURN(nvs_flash_erase(), "Failed to erase NVS");
        ret = nvs_flash_init();
    }
    CHECK_ERROR_RETURN(ret, "Failed to init NVS");

    // Open NVS handle
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &storage_handle);
    CHECK_ERROR_RETURN(ret, "Failed to open NVS handle");

    return ESP_OK;
}

esp_err_t config_load(panel_config_t* config)
{
    esp_err_t ret;
    size_t required_size;

    // Start with defaults
    memcpy(config, &default_config, sizeof(panel_config_t));

    bool updated = false;

    // Load WiFi config from environment variables, falling back to NVS
    const char* env = getenv("WIFI_SSID");
    if (env != NULL && env[0] != '\0')
    {
        strncpy(config->wifi_ssid, env, sizeof(config->wifi_ssid) - 1);
        config->wifi_ssid[sizeof(config->wifi_ssid) - 1] = '\0';
        nvs_set_str(storage_handle, NVS_KEY_WIFI_CONFIG ".ssid", config->wifi_ssid);
        updated = true;
    }
    else
    {
        required_size = sizeof(config->wifi_ssid);
        ret = nvs_get_str(storage_handle, NVS_KEY_WIFI_CONFIG ".ssid", config->wifi_ssid,
                          &required_size);
        if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Error reading WiFi SSID: %s", esp_err_to_name(ret));
        }
    }

    env = getenv("WIFI_PASSWORD");
    if (env != NULL && env[0] != '\0')
    {
        strncpy(config->wifi_password, env, sizeof(config->wifi_password) - 1);
        config->wifi_password[sizeof(config->wifi_password) - 1] = '\0';
        nvs_set_str(storage_handle, NVS_KEY_WIFI_CONFIG ".pass", config->wifi_password);
        updated = true;
    }
    else
    {
        required_size = sizeof(config->wifi_password);
        ret = nvs_get_str(storage_handle, NVS_KEY_WIFI_CONFIG ".pass", config->wifi_password,
                          &required_size);
        if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Error reading WiFi password: %s", esp_err_to_name(ret));
        }
    }

    // Load Home Assistant config from environment variables first, falling back to NVS
    env = getenv("HA_URL");
    if (env != NULL && env[0] != '\0')
    {
        strncpy(config->ha_url, env, sizeof(config->ha_url) - 1);
        config->ha_url[sizeof(config->ha_url) - 1] = '\0';
        ret = nvs_set_str(storage_handle, NVS_KEY_HA_CONFIG ".url", config->ha_url);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Error saving HA URL to NVS: %s", esp_err_to_name(ret));
        }
        else
        {
            updated = true;
        }
    }
    else
    {
        const char* base_env = getenv("HA_BASE_URL");
        if (base_env != NULL && base_env[0] != '\0')
        {
            strncpy(config->ha_url, base_env, sizeof(config->ha_url) - 1);
            config->ha_url[sizeof(config->ha_url) - 1] = '\0';
            ret = nvs_set_str(storage_handle, NVS_KEY_HA_CONFIG ".url", config->ha_url);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Error saving HA URL to NVS: %s", esp_err_to_name(ret));
            }
            else
            {
                updated = true;
            }
        }
        else
        {
            required_size = sizeof(config->ha_url);
            ret = nvs_get_str(storage_handle, NVS_KEY_HA_CONFIG ".url", config->ha_url,
                              &required_size);
            if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND)
            {
                ESP_LOGE(TAG, "Error reading HA URL: %s", esp_err_to_name(ret));
            }
        }
    }

    env = getenv("HA_TOKEN");
    if (env != NULL && env[0] != '\0')
    {
        strncpy(config->ha_token, env, sizeof(config->ha_token) - 1);
        config->ha_token[sizeof(config->ha_token) - 1] = '\0';
        ret = nvs_set_str(storage_handle, NVS_KEY_HA_CONFIG ".token", config->ha_token);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Error saving HA token to NVS: %s", esp_err_to_name(ret));
        }
        else
        {
            updated = true;
        }
    }
    else
    {
        required_size = sizeof(config->ha_token);
        ret = nvs_get_str(storage_handle, NVS_KEY_HA_CONFIG ".token", config->ha_token,
                          &required_size);
        if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Error reading HA token: %s", esp_err_to_name(ret));
        }
    }

    uint8_t use_https;
    ret = nvs_get_u8(storage_handle, NVS_KEY_HA_CONFIG ".https", &use_https);
    if (ret == ESP_OK)
    {
        config->use_https = use_https;
    }

    uint16_t update_interval;
    ret = nvs_get_u16(storage_handle, NVS_KEY_HA_CONFIG ".interval", &update_interval);
    if (ret == ESP_OK)
    {
        config->update_interval_ms = update_interval;
    }

    if (updated)
    {
        nvs_commit(storage_handle);
    }

    // Fall back to compile-time defaults if NVS and env vars yielded nothing
    if (config->ha_url[0] == '\0' && PANEL_DEFAULT_HA_URL[0] != '\0')
    {
        strncpy(config->ha_url, PANEL_DEFAULT_HA_URL, sizeof(config->ha_url) - 1);
        ESP_LOGI(TAG, "Using compile-time HA URL: %s", config->ha_url);
    }
    if (config->ha_token[0] == '\0' && PANEL_DEFAULT_HA_TOKEN[0] != '\0')
    {
        strncpy(config->ha_token, PANEL_DEFAULT_HA_TOKEN, sizeof(config->ha_token) - 1);
        ESP_LOGI(TAG, "Using compile-time HA token");
    }
    if (config->ha_url[0] == '\0' || config->ha_token[0] == '\0')
    {
        ESP_LOGW(TAG, "Home Assistant credentials not set; device requires provisioning");
    }

    return ESP_OK;
}

esp_err_t config_save(const panel_config_t* config)
{
    esp_err_t ret;

    // Save WiFi config
    ret = nvs_set_str(storage_handle, NVS_KEY_WIFI_CONFIG ".ssid", config->wifi_ssid);
    CHECK_ERROR_RETURN(ret, "Failed to save WiFi SSID");

    ret = nvs_set_str(storage_handle, NVS_KEY_WIFI_CONFIG ".pass", config->wifi_password);
    CHECK_ERROR_RETURN(ret, "Failed to save WiFi password");

    // Save HA config
    ret = nvs_set_str(storage_handle, NVS_KEY_HA_CONFIG ".url", config->ha_url);
    CHECK_ERROR_RETURN(ret, "Failed to save HA URL");

    ret = nvs_set_str(storage_handle, NVS_KEY_HA_CONFIG ".token", config->ha_token);
    CHECK_ERROR_RETURN(ret, "Failed to save HA token");

    ret = nvs_set_u8(storage_handle, NVS_KEY_HA_CONFIG ".https", config->use_https);
    CHECK_ERROR_RETURN(ret, "Failed to save HTTPS flag");

    ret = nvs_set_u16(storage_handle, NVS_KEY_HA_CONFIG ".interval", config->update_interval_ms);
    CHECK_ERROR_RETURN(ret, "Failed to save update interval");

    // Commit changes
    ret = nvs_commit(storage_handle);
    CHECK_ERROR_RETURN(ret, "Failed to commit NVS changes");

    return ESP_OK;
}

esp_err_t config_reset_to_defaults(void)
{
    esp_err_t ret;

    // Erase all keys in our namespace
    ret = nvs_erase_all(storage_handle);
    CHECK_ERROR_RETURN(ret, "Failed to erase NVS namespace");

    // Commit the erasure
    ret = nvs_commit(storage_handle);
    CHECK_ERROR_RETURN(ret, "Failed to commit NVS erasure");

    return ESP_OK;
}

esp_err_t config_load_display(display_config_t* config)
{
    esp_err_t ret;

    // Start with defaults
    memcpy(config, &default_display_config, sizeof(display_config_t));

    // Load brightness
    uint8_t brightness;
    ret = nvs_get_u8(storage_handle, NVS_KEY_DISPLAY_CONFIG ".brightness", &brightness);
    if (ret == ESP_OK)
    {
        config->brightness = brightness;
    }

    // Load auto-dim timeout
    uint16_t timeout;
    ret = nvs_get_u16(storage_handle, NVS_KEY_DISPLAY_CONFIG ".dim_timeout", &timeout);
    if (ret == ESP_OK)
    {
        config->auto_dim_timeout = timeout;
    }

    // Load auto-dim enabled flag
    uint8_t enabled;
    ret = nvs_get_u8(storage_handle, NVS_KEY_DISPLAY_CONFIG ".dim_enabled", &enabled);
    if (ret == ESP_OK)
    {
        config->auto_dim_enabled = enabled;
    }

    // Load dim level
    uint8_t dim_level;
    ret = nvs_get_u8(storage_handle, NVS_KEY_DISPLAY_CONFIG ".dim_level", &dim_level);
    if (ret == ESP_OK)
    {
        config->dim_level = dim_level;
    }

    return ESP_OK;
}

esp_err_t config_save_display(const display_config_t* config)
{
    esp_err_t ret;

    // Save brightness
    ret = nvs_set_u8(storage_handle, NVS_KEY_DISPLAY_CONFIG ".brightness", config->brightness);
    CHECK_ERROR_RETURN(ret, "Failed to save brightness");

    // Save auto-dim timeout
    ret = nvs_set_u16(storage_handle, NVS_KEY_DISPLAY_CONFIG ".dim_timeout",
                      config->auto_dim_timeout);
    CHECK_ERROR_RETURN(ret, "Failed to save dim timeout");

    // Save auto-dim enabled flag
    ret =
        nvs_set_u8(storage_handle, NVS_KEY_DISPLAY_CONFIG ".dim_enabled", config->auto_dim_enabled);
    CHECK_ERROR_RETURN(ret, "Failed to save dim enabled");

    // Save dim level
    ret = nvs_set_u8(storage_handle, NVS_KEY_DISPLAY_CONFIG ".dim_level", config->dim_level);
    CHECK_ERROR_RETURN(ret, "Failed to save dim level");

    // Commit changes
    ret = nvs_commit(storage_handle);
    CHECK_ERROR_RETURN(ret, "Failed to commit display config changes");

    return ESP_OK;
}
