#include "ha_client.h"
#include "cJSON.h"
#include <string.h>

static bool s_initialized;
static bool s_connected;
static cJSON* s_states;

esp_err_t ha_client_init(const ha_client_config_t* config)
{
    if (!config)
    {
        return ESP_ERR_INVALID_ARG;
    }
    s_initialized = true;
    return ESP_OK;
}

esp_err_t ha_client_connect(void)
{
    if (!s_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }
    s_connected = true;
    return ESP_OK;
}

esp_err_t ha_client_disconnect(void)
{
    if (!s_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }
    s_connected = false;
    return ESP_OK;
}

esp_err_t ha_client_subscribe(const char* entity_id)
{
    if (!s_connected)
    {
        return ESP_ERR_INVALID_STATE;
    }
    return entity_id ? ESP_OK : ESP_ERR_INVALID_ARG;
}

esp_err_t ha_client_put(const char* path, const cJSON* payload)
{
    (void)payload;
    if (!s_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }
    return path ? ESP_OK : ESP_ERR_INVALID_ARG;
}

void ha_client_register_callback(ha_event_callback_t callback, void* user_data)
{
    (void)callback;
    (void)user_data;
}

void mock_ha_client_set_states_json(const char* json)
{
    if (s_states)
    {
        cJSON_Delete(s_states);
        s_states = NULL;
    }
    if (json)
    {
        s_states = cJSON_Parse(json);
    }
}

esp_err_t ha_client_get_states(cJSON** out_states)
{
    if (!out_states || !s_states)
    {
        return ESP_FAIL;
    }
    *out_states = cJSON_Duplicate(s_states, true);
    return *out_states ? ESP_OK : ESP_FAIL;
}

void ha_client_deinit(void)
{
    s_initialized = false;
    s_connected = false;
    if (s_states)
    {
        cJSON_Delete(s_states);
        s_states = NULL;
    }
}
