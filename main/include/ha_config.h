#pragma once

#include <stdint.h>
#include <stdbool.h>

// Maximum lengths for connection parameters
#define HA_MAX_BASE_URL_LEN 128
#define HA_MAX_TOKEN_LEN 256
#define HA_MAX_USER_AGENT_LEN 64
#define HA_MAX_ENTITY_ID_LEN 64

typedef struct
{
    const char* base_url;
    const char* token;
    uint16_t port;
    const char* user_agent;
    uint32_t timeout_ms;
    uint8_t max_retries;
    bool use_ssl;    // true to use HTTPS instead of HTTP
    bool verify_ssl; // true to verify server certificate
} ha_config_t;

// Alias for plan naming consistency
typedef ha_config_t ha_client_cfg_t;

// Global Home Assistant configuration (defined in ha_client.c)
extern ha_config_t g_ha_config;

#define HA_DEFAULT_TIMEOUT_MS 5000
#define HA_DEFAULT_MAX_RETRIES 3
#define HA_MAX_RETRIES 10
#define HA_DEFAULT_VERIFY_SSL 1
#define HA_DEFAULT_PORT 8123

#define HA_HTTP_HEADER_AUTH "Authorization"
#define HA_HTTP_HEADER_JSON "Content-Type: application/json"
#define HA_HTTP_HEADER_ACCEPT "Accept"
#define HA_HTTP_HEADER_USER_AGENT "User-Agent"
#define HA_HTTP_HEADER_CONTENT_LENGTH "Content-Length"
#define HA_HTTP_CONTENT_JSON "application/json"
#define HA_HTTP_BEARER_PREFIX "Bearer "
#define HA_MAX_AUTH_HEADER_LEN (7 + HA_MAX_TOKEN_LEN)

// Base path for Home Assistant REST API and common endpoints
#define HA_API_BASE "/api"
#define HA_API_STATES HA_API_BASE "/states"
#define HA_API_STATE_FMT HA_API_BASE "/states/%s"
#define HA_API_SERVICES_BASE HA_API_BASE "/services"
#define HA_API_DOMAIN_FMT HA_API_SERVICES_BASE "/%s"
#define HA_API_LIGHT_TURN_ON HA_API_SERVICES_BASE "/light/turn_on"
#define HA_API_LIGHT_TURN_OFF HA_API_SERVICES_BASE "/light/turn_off"
#define HA_API_SCENE_TURN_ON HA_API_SERVICES_BASE "/scene/turn_on"
#define HA_API_AUTOMATION_TRIGGER HA_API_SERVICES_BASE "/automation/trigger"
#define HA_API_SERVICE_FMT HA_API_SERVICES_BASE "/%s/%s"

// Config registry endpoints (used by discovery module)
#define HA_API_ENTITY_REGISTRY "/api/config/entity_registry/list"
#define HA_API_AREA_REGISTRY "/api/config/area_registry/list"

// Service domain helpers
#define HA_DOMAIN_LIGHT_STR "light"
#define HA_DOMAIN_SCENE_STR "scene"
#define HA_DOMAIN_AUTOMATION_STR "automation"

typedef enum
{
    HA_DOMAIN_LIGHT,
    HA_DOMAIN_SCENE,
    HA_DOMAIN_AUTOMATION,
} ha_domain_t;

#define HA_DEFAULT_USE_SSL 0
#ifndef HA_USE_SSL
#define HA_USE_SSL HA_DEFAULT_USE_SSL
#endif
#ifndef HA_VERIFY_SSL
#define HA_VERIFY_SSL HA_DEFAULT_VERIFY_SSL
#endif
#ifndef HA_PORT
#define HA_PORT HA_DEFAULT_PORT
#endif

// Home Assistant connection parameters can be provisioned at runtime
#ifndef HA_BASE_URL
#define HA_BASE_URL ""
#endif

#ifndef HA_TOKEN
#define HA_TOKEN ""
#endif

#define HA_DEFAULT_USER_AGENT "ESP32-TouchPanel"

#ifndef HA_USER_AGENT
#define HA_USER_AGENT HA_DEFAULT_USER_AGENT
#endif

// Initialize a config structure with project defaults
void ha_config_init(ha_config_t* config);
