// Real implementation is excluded during unit tests to allow mocking.
#include "ha_client.h"

#ifdef UNIT_TEST
// In test builds, functions are provided by mock_ha_client.c
#else
#include "http_client.h"
#include "esp_websocket_client.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char* TAG = "ha_client";

#ifndef UNIT_TEST
static char g_base_url[HA_MAX_BASE_URL_LEN] = "";
static char g_token[HA_MAX_TOKEN_LEN] = "";

ha_config_t g_ha_config = {
    .base_url = g_base_url,
    .token = g_token,
    .port = HA_PORT,
    .user_agent = HA_USER_AGENT,
    .timeout_ms = HA_DEFAULT_TIMEOUT_MS,
    .max_retries = HA_DEFAULT_MAX_RETRIES,
    .use_ssl = HA_USE_SSL,
    .verify_ssl = HA_VERIFY_SSL,
};
#endif

static void ha_client_init_http(void)
{
    http_client_init(g_ha_config.base_url, g_ha_config.token, g_ha_config.use_ssl);
}

void ha_config_init(ha_config_t* config)
{
    if (!config)
    {
        return;
    }
    config->base_url = "";
    config->token = "";
    config->port = HA_PORT;
    config->user_agent = HA_USER_AGENT;
    config->timeout_ms = HA_DEFAULT_TIMEOUT_MS;
    config->max_retries = HA_DEFAULT_MAX_RETRIES;
    config->use_ssl = HA_USE_SSL;
    config->verify_ssl = HA_VERIFY_SSL;
}

void ha_client_set_config(const ha_config_t* config)
{
    if (config)
    {
        g_ha_config = *config;
        ha_client_apply_config();
    }
}

void ha_client_apply_config(void) { ha_client_init_http(); }

void ha_client_set_base_url(const char* base_url)
{
    if (base_url)
    {
        g_ha_config.base_url = base_url;
        if (http_client_is_configured())
        {
            ha_client_init_http();
        }
    }
}

void ha_client_set_token(const char* token)
{
    if (token)
    {
        g_ha_config.token = token;
        if (http_client_is_configured())
        {
            ha_client_init_http();
        }
    }
}

void ha_client_set_port(uint16_t port)
{
    g_ha_config.port = port;
    if (http_client_is_configured())
    {
        ha_client_init_http();
    }
}

void ha_client_set_user_agent(const char* user_agent)
{
    if (user_agent)
    {
        g_ha_config.user_agent = user_agent;
    }
}

void ha_client_set_timeout(uint32_t timeout_ms) { g_ha_config.timeout_ms = timeout_ms; }

void ha_client_set_max_retries(uint8_t max_retries)
{
    g_ha_config.max_retries = max_retries > HA_MAX_RETRIES ? HA_MAX_RETRIES : max_retries;
}

void ha_client_set_use_ssl(bool use_ssl)
{
    g_ha_config.use_ssl = use_ssl;
    if (http_client_is_configured())
    {
        ha_client_init_http();
    }
}

void ha_client_set_verify_ssl(bool verify_ssl)
{
    g_ha_config.verify_ssl = verify_ssl;
    if (http_client_is_configured())
    {
        ha_client_init_http();
    }
}

bool ha_client_is_configured(void)
{
    return http_client_is_configured() && g_ha_config.token && g_ha_config.token[0] != '\0';
}

const char* ha_client_get_auth_header(void)
{
    static char header[HA_MAX_AUTH_HEADER_LEN + 1];
    if (!g_ha_config.token)
    {
        header[0] = '\0';
        return header;
    }
    snprintf(header, sizeof(header), "%s%s", HA_HTTP_BEARER_PREFIX, g_ha_config.token);
    return header;
}

static const char* ha_domain_str(ha_domain_t domain)
{
    switch (domain)
    {
    case HA_DOMAIN_LIGHT:
        return HA_DOMAIN_LIGHT_STR;
    case HA_DOMAIN_SCENE:
        return HA_DOMAIN_SCENE_STR;
    case HA_DOMAIN_AUTOMATION:
        return HA_DOMAIN_AUTOMATION_STR;
    default:
        return "";
    }
}

#define MAX_SUBSCRIPTIONS 32
#define MAX_PAYLOAD_SIZE 2048
#define QUEUE_SIZE 10
#define AUTH_TIMEOUT_MS 5000
#define RECONNECT_TIMEOUT_MS 5000

typedef struct
{
    ha_client_config_t config;
    esp_websocket_client_handle_t ws_client;
    esp_http_client_handle_t http_client;
    ha_ws_state_t ws_state;
    ha_event_callback_t event_callback;
    void* callback_arg;
    QueueHandle_t event_queue;
    SemaphoreHandle_t state_mutex;
    uint32_t message_id;
    char* subscriptions[MAX_SUBSCRIPTIONS];
    size_t subscription_count;
    bool running;
} ha_client_t;

static ha_client_t* client = NULL;

// Forward declarations
static void ws_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id,
                             void* event_data);
static void process_event_queue_task(void* pvParameters);
static esp_err_t send_auth_message(void);
static esp_err_t send_subscription_message(const char* entity_id);
static void handle_state_change(const cJSON* event);

esp_err_t ha_client_setup(void)
{
    panel_config_t cfg;
    esp_err_t ret = config_load(&cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to load config: %s", esp_err_to_name(ret));
        return ret;
    }

    strncpy(g_base_url, cfg.ha_url, sizeof(g_base_url) - 1);
    g_base_url[sizeof(g_base_url) - 1] = '\0';

    strncpy(g_token, cfg.ha_token, sizeof(g_token) - 1);
    g_token[sizeof(g_token) - 1] = '\0';

    g_ha_config.use_ssl = cfg.use_https;

    bool dirty = false;
    const char* env = getenv("HA_BASE_URL");
    if (env && env[0] != '\0')
    {
        strncpy(g_base_url, env, sizeof(g_base_url) - 1);
        g_base_url[sizeof(g_base_url) - 1] = '\0';
        strncpy(cfg.ha_url, env, sizeof(cfg.ha_url) - 1);
        cfg.ha_url[sizeof(cfg.ha_url) - 1] = '\0';
        dirty = true;
    }

    env = getenv("HA_TOKEN");
    if (env && env[0] != '\0')
    {
        strncpy(g_token, env, sizeof(g_token) - 1);
        g_token[sizeof(g_token) - 1] = '\0';
        strncpy(cfg.ha_token, env, sizeof(cfg.ha_token) - 1);
        cfg.ha_token[sizeof(cfg.ha_token) - 1] = '\0';
        dirty = true;
    }

    if (dirty)
    {
        config_save(&cfg);
    }

    ha_client_init_http();
    return ESP_OK;
}

esp_err_t ha_client_init(const ha_client_config_t* config)
{
    if (client != NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    client = calloc(1, sizeof(ha_client_t));
    if (client == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate client context");
        return ESP_ERR_NO_MEM;
    }

    // Copy configuration
    memcpy(&client->config, config, sizeof(ha_client_config_t));

    // Create event queue and mutex
    client->event_queue = xQueueCreate(QUEUE_SIZE, sizeof(ha_event_t));
    client->state_mutex = xSemaphoreCreateMutex();
    if (client->event_queue == NULL || client->state_mutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create queue or mutex");
        if (client->event_queue)
        {
            vQueueDelete(client->event_queue);
        }
        if (client->state_mutex)
        {
            vSemaphoreDelete(client->state_mutex);
        }
        free(client);
        client = NULL;
        return ESP_ERR_NO_MEM;
    }

    // Initialize WebSocket client
    char ws_url[256];
    if (!http_client_is_configured())
    {
        return ESP_ERR_INVALID_STATE;
    }
    const char* base = http_client_get_base_url();
    const char* host = base;
    if (strncmp(base, "https://", 8) == 0)
    {
        host = base + 8;
    }
    else if (strncmp(base, "http://", 7) == 0)
    {
        host = base + 7;
    }
    bool has_port = strchr(host, ':') != NULL;
    const char* scheme = config->use_ssl ? "wss" : "ws";
    if (!has_port && g_ha_config.port)
    {
        snprintf(ws_url, sizeof(ws_url), "%s://%s:%u/api/websocket", scheme, host,
                 g_ha_config.port);
    }
    else
    {
        snprintf(ws_url, sizeof(ws_url), "%s://%s/api/websocket", scheme, host);
    }

    esp_websocket_client_config_t ws_config = {
        .uri = ws_url,
        .disable_auto_reconnect = true, // We'll handle reconnection
        .transport = config->use_ssl ? WEBSOCKET_TRANSPORT_OVER_SSL : WEBSOCKET_TRANSPORT_OVER_TCP,
        .user_agent = "ESP32-TouchPanel",
        .buffer_size = MAX_PAYLOAD_SIZE,
        .crt_bundle_attach = config->use_ssl ? esp_crt_bundle_attach : NULL,
    };

    client->ws_client = esp_websocket_client_init(&ws_config);
    if (client->ws_client == NULL)
    {
        ESP_LOGE(TAG, "Failed to init WebSocket client");
        vQueueDelete(client->event_queue);
        vSemaphoreDelete(client->state_mutex);
        free(client);
        client = NULL;
        return ESP_FAIL;
    }

    // Register WebSocket event handler
    esp_websocket_register_events(client->ws_client, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL);

    // Start event processing task
    client->running = true;
    xTaskCreate(process_event_queue_task, "ha_event_task", 4096, NULL, 5, NULL);

    return ESP_OK;
}

esp_err_t ha_client_connect(void)
{
    if (client == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (client->ws_state != HA_WS_DISCONNECTED)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_websocket_client_start(client->ws_client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "WebSocket start failed: %s", esp_err_to_name(err));
        return err;
    }

    client->ws_state = HA_WS_CONNECTING;
    return ESP_OK;
}

static void ws_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id,
                             void* event_data)
{
    esp_websocket_event_data_t* data = (esp_websocket_event_data_t*)event_data;

    switch (event_id)
    {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WebSocket connected");
        send_auth_message();
        break;

    case WEBSOCKET_EVENT_DATA:
        if (data->op_code == 1)
        { // Text frame
            // Parse JSON message
            cJSON* root = cJSON_ParseWithLength(data->data_ptr, data->data_len);
            if (root)
            {
                const cJSON* type = cJSON_GetObjectItem(root, "type");
                if (cJSON_IsString(type))
                {
                    if (strcmp(type->valuestring, "auth_ok") == 0)
                    {
                        client->ws_state = HA_WS_CONNECTED;
                        // Resubscribe to entities
                        for (size_t i = 0; i < client->subscription_count; i++)
                        {
                            send_subscription_message(client->subscriptions[i]);
                        }
                    }
                    else if (strcmp(type->valuestring, "event") == 0)
                    {
                        handle_state_change(root);
                    }
                }
                cJSON_Delete(root);
            }
        }
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "WebSocket disconnected");
        client->ws_state = HA_WS_DISCONNECTED;
        break;

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WebSocket error");
        client->ws_state = HA_WS_ERROR;
        break;
    }
}

static esp_err_t send_auth_message(void)
{
    cJSON* root = cJSON_CreateObject();
    if (root == NULL)
    {
        ESP_LOGE(TAG, "Failed to create auth JSON");
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(root, "type", "auth");
    cJSON_AddStringToObject(root, "access_token", http_client_get_token());

    char* message = cJSON_PrintUnformatted(root);
    if (message == NULL)
    {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Failed to serialize auth JSON");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret =
        esp_websocket_client_send_text(client->ws_client, message, strlen(message), portMAX_DELAY);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send auth message: %s", esp_err_to_name(ret));
    }

    free(message);
    cJSON_Delete(root);

    return ret;
}

static esp_err_t send_subscription_message(const char* entity_id)
{
    cJSON* root = cJSON_CreateObject();
    if (root == NULL)
    {
        ESP_LOGE(TAG, "Failed to create subscribe JSON");
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(root, "type", "subscribe_events");
    cJSON_AddStringToObject(root, "event_type", "state_changed");
    cJSON_AddNumberToObject(root, "id", ++client->message_id);

    char* message = cJSON_PrintUnformatted(root);
    if (message == NULL)
    {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Failed to serialize subscribe JSON");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret =
        esp_websocket_client_send_text(client->ws_client, message, strlen(message), portMAX_DELAY);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send subscribe message: %s", esp_err_to_name(ret));
    }

    free(message);
    cJSON_Delete(root);

    return ret;
}

static void handle_state_change(const cJSON* event)
{
    const cJSON* data = cJSON_GetObjectItem(event, "event");
    if (!data)
        return;

    const cJSON* entity_data = cJSON_GetObjectItem(data, "data");
    if (!entity_data)
        return;

    const cJSON* entity_id = cJSON_GetObjectItem(entity_data, "entity_id");
    const cJSON* new_state = cJSON_GetObjectItem(entity_data, "new_state");

    if (entity_id && new_state)
    {
        ha_event_t ha_event = {.type = HA_EVENT_STATE_CHANGED};

        strncpy(ha_event.state.entity_id, entity_id->valuestring,
                sizeof(ha_event.state.entity_id) - 1);

        const cJSON* state = cJSON_GetObjectItem(new_state, "state");
        if (state && cJSON_IsString(state))
        {
            strncpy(ha_event.state.state, state->valuestring, sizeof(ha_event.state.state) - 1);
        }

        // Queue the event
        xQueueSend(client->event_queue, &ha_event, 0);
    }
}

static void process_event_queue_task(void* pvParameters)
{
    ha_event_t event;

    while (client->running)
    {
        if (xQueueReceive(client->event_queue, &event, pdMS_TO_TICKS(100)))
        {
            if (client->event_callback)
            {
                client->event_callback(&event, client->callback_arg);
            }
        }
    }

    vTaskDelete(NULL);
}

esp_err_t ha_client_subscribe(const char* entity_id)
{
    if (client == NULL || entity_id == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (client->subscription_count >= client->config.max_subscriptions)
    {
        return ESP_ERR_NO_MEM;
    }

    // Add to subscription list
    client->subscriptions[client->subscription_count] = strdup(entity_id);
    if (client->subscriptions[client->subscription_count] == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate subscription entry");
        return ESP_ERR_NO_MEM;
    }
    client->subscription_count++;

    // If connected, send subscription message
    if (client->ws_state == HA_WS_CONNECTED)
    {
        return send_subscription_message(entity_id);
    }

    return ESP_OK;
}

void ha_client_register_callback(ha_event_callback_t callback, void* user_data)
{
    if (client != NULL)
    {
        client->event_callback = callback;
        client->callback_arg = user_data;
    }
}

esp_err_t ha_client_disconnect(void)
{
    if (client == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_websocket_client_stop(client->ws_client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "WebSocket stop failed: %s", esp_err_to_name(err));
        return err;
    }

    client->ws_state = HA_WS_DISCONNECTED;
    return ESP_OK;
}

void ha_client_deinit(void)
{
    if (client == NULL)
    {
        return;
    }

    client->running = false;

    if (client->ws_client)
    {
        esp_websocket_client_stop(client->ws_client);
        esp_websocket_client_destroy(client->ws_client);
    }

    for (size_t i = 0; i < client->subscription_count; i++)
    {
        free(client->subscriptions[i]);
    }

    if (client->event_queue)
    {
        vQueueDelete(client->event_queue);
    }
    if (client->state_mutex)
    {
        vSemaphoreDelete(client->state_mutex);
    }

    free(client);
    client = NULL;
}

esp_err_t ha_client_get(const char* path, cJSON** out_json)
{
    if (!ha_client_is_configured())
    {
        ESP_LOGE(TAG, "HA client not configured");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGD(TAG, "GET %s", path);
    char* buf = NULL;
    size_t len = 0;
    if (!http_client_get(path, &buf, &len))
    {
        ESP_LOGE(TAG, "GET %s failed", path);
        return ESP_FAIL;
    }
    if (out_json)
    {
        *out_json = NULL;
        cJSON* parsed = cJSON_ParseWithLength(buf, len);
        free(buf);
        if (!parsed)
        {
            ESP_LOGE(TAG, "GET %s returned invalid JSON (length %zu)", path, len);
            return ESP_FAIL;
        }
        *out_json = parsed;
    }
    else
    {
        free(buf);
    }
    return ESP_OK;
}

esp_err_t ha_client_post(const char* path, const cJSON* payload)
{
    if (!ha_client_is_configured())
    {
        ESP_LOGE(TAG, "HA client not configured");
        return ESP_ERR_INVALID_STATE;
    }
    char* body = NULL;
    if (payload)
    {
        body = cJSON_PrintUnformatted((cJSON*)payload);
        if (!body)
        {
            return ESP_ERR_NO_MEM;
        }
    }
    ESP_LOGD(TAG, "POST %s", path);
    bool ok = http_client_post(path, body ? body : "{}");
    if (!ok)
    {
        ESP_LOGE(TAG, "POST %s failed", path);
    }
    if (body)
    {
        cJSON_free(body);
    }
    return ok ? ESP_OK : ESP_FAIL;
}

esp_err_t ha_client_put(const char* path, const cJSON* payload)
{
    if (!ha_client_is_configured())
    {
        ESP_LOGE(TAG, "HA client not configured");
        return ESP_ERR_INVALID_STATE;
    }
    char* body = NULL;
    if (payload)
    {
        body = cJSON_PrintUnformatted((cJSON*)payload);
        if (!body)
        {
            return ESP_ERR_NO_MEM;
        }
    }
    ESP_LOGD(TAG, "PUT %s", path);
    bool ok = http_client_put(path, body ? body : "{}");
    if (!ok)
    {
        ESP_LOGE(TAG, "PUT %s failed", path);
    }
    if (body)
    {
        cJSON_free(body);
    }
    return ok ? ESP_OK : ESP_FAIL;
}

esp_err_t ha_client_delete(const char* path)
{
    return ha_client_request("DELETE", path, NULL, NULL);
}

esp_err_t ha_client_request(const char* method, const char* path, const cJSON* payload,
                            cJSON** out_json)
{
    if (!method || !path)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (strcasecmp(method, "GET") == 0)
    {
        return ha_client_get(path, out_json);
    }
    if (strcasecmp(method, "POST") == 0)
    {
        if (out_json)
        {
            *out_json = NULL;
        }
        return ha_client_post(path, payload);
    }
    if (strcasecmp(method, "PUT") == 0)
    {
        if (out_json)
        {
            *out_json = NULL;
        }
        return ha_client_put(path, payload);
    }
    if (strcasecmp(method, "DELETE") == 0)
    {
        if (out_json)
        {
            *out_json = NULL;
        }
        return ha_client_delete(path);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t ha_client_call_service_path(const char* path, const cJSON* payload)
{
    if (!path)
    {
        return ESP_ERR_INVALID_ARG;
    }
    const cJSON* body = payload ? payload : cJSON_CreateObject();
    esp_err_t err = ha_client_post(path, body);
    if (!payload)
    {
        cJSON_Delete((cJSON*)body);
    }
    return err;
}

esp_err_t ha_client_call_service(const char* domain, const char* service, const cJSON* payload)
{
    if (!domain || !service)
    {
        return ESP_ERR_INVALID_ARG;
    }
    char path[128];
    snprintf(path, sizeof(path), HA_API_SERVICE_FMT, domain, service);
    const cJSON* body = payload ? payload : cJSON_CreateObject();
    esp_err_t err = ha_client_post(path, body);
    if (!payload)
    {
        cJSON_Delete((cJSON*)body);
    }
    return err;
}

esp_err_t ha_client_call_service_domain(ha_domain_t domain, const char* service,
                                        const cJSON* payload)
{
    const char* domain_str = ha_domain_str(domain);
    if (!domain_str[0])
    {
        return ESP_ERR_INVALID_ARG;
    }
    return ha_client_call_service(domain_str, service, payload);
}

esp_err_t ha_client_get_states(cJSON** out_states)
{
    if (!out_states)
    {
        return ESP_ERR_INVALID_ARG;
    }
    return ha_client_get(HA_API_STATES, out_states);
}

esp_err_t ha_client_get_state(const char* entity_id, cJSON** out_state)
{
    if (!entity_id || !out_state)
    {
        return ESP_ERR_INVALID_ARG;
    }
    char path[128];
    snprintf(path, sizeof(path), HA_API_STATE_FMT, entity_id);
    return ha_client_get(path, out_state);
}

esp_err_t ha_client_get_entity_registry(cJSON** out_json)
{
    if (!out_json)
        return ESP_ERR_INVALID_ARG;
    return ha_client_get(HA_API_ENTITY_REGISTRY, out_json);
}

esp_err_t ha_client_get_area_registry(cJSON** out_json)
{
    if (!out_json)
        return ESP_ERR_INVALID_ARG;
    return ha_client_get(HA_API_AREA_REGISTRY, out_json);
}

esp_err_t ha_client_light_turn_on(const char* entity_id, const cJSON* params)
{
    ESP_LOGI(TAG, ">>> ha_client_light_turn_on: entity='%s'", entity_id ? entity_id : "NULL");

    if (!entity_id)
    {
        ESP_LOGE(TAG, "<<< ha_client_light_turn_on: NULL entity_id");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "entity_id", entity_id);

    // Merge params into root (not nested under "data")
    if (params)
    {
        ESP_LOGI(TAG, "Merging params into request body");
        cJSON* item = params->child;
        while (item)
        {
            cJSON* dup = cJSON_Duplicate(item, 1);
            cJSON_AddItemToObject(root, item->string, dup);
            item = item->next;
        }
    }

    char* json_str = cJSON_Print(root);
    ESP_LOGI(TAG, "POST %s with body: %s", HA_API_LIGHT_TURN_ON, json_str ? json_str : "NULL");
    if (json_str)
    {
        cJSON_free(json_str);
    }

    esp_err_t err = ha_client_post(HA_API_LIGHT_TURN_ON, root);
    ESP_LOGI(TAG, "<<< ha_client_light_turn_on result: %s", esp_err_to_name(err));
    cJSON_Delete(root);
    return err;
}

esp_err_t ha_client_light_turn_off(const char* entity_id)
{
    ESP_LOGI(TAG, ">>> ha_client_light_turn_off: entity='%s'", entity_id ? entity_id : "NULL");

    if (!entity_id)
    {
        ESP_LOGE(TAG, "<<< ha_client_light_turn_off: NULL entity_id");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "entity_id", entity_id);

    char* json_str = cJSON_Print(root);
    ESP_LOGI(TAG, "POST %s with body: %s", HA_API_LIGHT_TURN_OFF, json_str ? json_str : "NULL");
    if (json_str)
    {
        cJSON_free(json_str);
    }

    esp_err_t err = ha_client_post(HA_API_LIGHT_TURN_OFF, root);
    ESP_LOGI(TAG, "<<< ha_client_light_turn_off result: %s", esp_err_to_name(err));
    cJSON_Delete(root);
    return err;
}

esp_err_t ha_client_scene_turn_on(const char* entity_id)
{
    if (!entity_id)
    {
        return ESP_ERR_INVALID_ARG;
    }
    cJSON* payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "entity_id", entity_id);
    esp_err_t err = ha_client_call_service("scene", "turn_on", payload);
    cJSON_Delete(payload);
    return err;
}

esp_err_t ha_client_automation_trigger(const char* entity_id)
{
    if (!entity_id)
    {
        return ESP_ERR_INVALID_ARG;
    }
    cJSON* payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "entity_id", entity_id);
    esp_err_t err = ha_client_call_service("automation", "trigger", payload);
    cJSON_Delete(payload);
    return err;
}

#endif // UNIT_TEST
