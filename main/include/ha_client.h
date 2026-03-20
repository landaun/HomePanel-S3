#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include "cJSON.h"
#include "ha_config.h"

// WebSocket message types
typedef enum
{
    HA_MSG_AUTH,
    HA_MSG_AUTH_OK,
    HA_MSG_AUTH_INVALID,
    HA_MSG_SUBSCRIBE,
    HA_MSG_EVENT,
    HA_MSG_RESULT,
    HA_MSG_ERROR
} ha_message_type_t;

// Entity state structure
typedef struct
{
    char entity_id[HA_MAX_ENTITY_ID_LEN];
    char state[32];
    char attributes[512];
    uint32_t last_updated;
} ha_entity_state_t;

// WebSocket connection states
typedef enum
{
    HA_WS_DISCONNECTED,
    HA_WS_CONNECTING,
    HA_WS_AUTHENTICATING,
    HA_WS_CONNECTED,
    HA_WS_ERROR
} ha_ws_state_t;

// Event callback types
typedef enum
{
    HA_EVENT_CONNECTED,
    HA_EVENT_DISCONNECTED,
    HA_EVENT_STATE_CHANGED,
    HA_EVENT_ERROR
} ha_event_type_t;

typedef struct
{
    ha_event_type_t type;
    union
    {
        ha_entity_state_t state;
        char error_msg[128];
    };
} ha_event_t;

// Callback types
typedef void (*ha_event_callback_t)(const ha_event_t* event, void* user_data);
typedef void (*ha_state_callback_t)(const ha_entity_state_t* state, void* user_data);

// Configuration structure
typedef struct
{
    bool use_ssl;             // Use SSL/TLS for connections
    uint16_t update_interval; // State update interval for REST fallback
    size_t max_subscriptions; // Maximum number of entity subscriptions
} ha_client_config_t;

// Function declarations
esp_err_t ha_client_init(const ha_client_config_t* config);
esp_err_t ha_client_connect(void);
esp_err_t ha_client_disconnect(void);
esp_err_t ha_client_subscribe(const char* entity_id);
void ha_client_register_callback(ha_event_callback_t callback, void* user_data);
void ha_client_deinit(void);

// Initialize REST client using g_ha_config
esp_err_t ha_client_setup(void);
void ha_client_set_config(const ha_config_t* config);
void ha_client_apply_config(void);
void ha_client_set_base_url(const char* base_url);
void ha_client_set_token(const char* token);
void ha_client_set_port(uint16_t port);
void ha_client_set_user_agent(const char* user_agent);
void ha_client_set_timeout(uint32_t timeout_ms);
void ha_client_set_max_retries(uint8_t max_retries);
void ha_client_set_use_ssl(bool use_ssl);
void ha_client_set_verify_ssl(bool verify_ssl);
bool ha_client_is_configured(void);

// Authorization header helper
const char* ha_client_get_auth_header(void);

// Access current Home Assistant configuration
static inline const ha_config_t* ha_client_get_config(void) { return &g_ha_config; }

static inline const char* ha_client_get_base_url(void) { return g_ha_config.base_url; }

static inline const char* ha_client_get_token(void) { return g_ha_config.token; }

static inline uint16_t ha_client_get_port(void) { return g_ha_config.port; }

static inline const char* ha_client_get_user_agent(void) { return g_ha_config.user_agent; }

static inline uint32_t ha_client_get_timeout(void) { return g_ha_config.timeout_ms; }

static inline uint8_t ha_client_get_max_retries(void) { return g_ha_config.max_retries; }

static inline bool ha_client_get_use_ssl(void) { return g_ha_config.use_ssl; }

static inline bool ha_client_get_verify_ssl(void) { return g_ha_config.verify_ssl; }

// Milestone 1 HTTP helper prototypes (use HA_API_* macros from ha_config.h)
esp_err_t ha_client_get_states(cJSON** out_states);
esp_err_t ha_client_get_state(const char* entity_id, cJSON** out_state);
esp_err_t ha_client_light_turn_on(const char* entity_id, const cJSON* params);
esp_err_t ha_client_light_turn_off(const char* entity_id);
esp_err_t ha_client_scene_turn_on(const char* entity_id);
esp_err_t ha_client_automation_trigger(const char* entity_id);
esp_err_t ha_client_call_service(const char* domain, const char* service, const cJSON* payload);
esp_err_t ha_client_call_service_path(const char* path, const cJSON* payload);
esp_err_t ha_client_call_service_domain(ha_domain_t domain, const char* service,
                                        const cJSON* payload);

// Entity and area registry endpoints (used by discovery module)
esp_err_t ha_client_get_entity_registry(cJSON** out_json);
esp_err_t ha_client_get_area_registry(cJSON** out_json);

// Generic REST helpers used by the above APIs
esp_err_t ha_client_get(const char* path, cJSON** out_json);
esp_err_t ha_client_post(const char* path, const cJSON* payload);
esp_err_t ha_client_put(const char* path, const cJSON* payload);
esp_err_t ha_client_delete(const char* path);
esp_err_t ha_client_request(const char* method, const char* path, const cJSON* payload,
                            cJSON** out_json);
