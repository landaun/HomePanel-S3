#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "cJSON.h"
#include "ha_config.h"

#define COMMAND_QUEUE_COALESCE_MS 120
#define COMMAND_QUEUE_LEN 10

typedef enum
{
    HA_CMD_SERVICE,
    HA_CMD_LIGHT,
    HA_CMD_LIGHT_OFF,
    HA_CMD_SCENE,
    HA_CMD_AUTOMATION,
    HA_CMD_REQUEST,
} ha_cmd_type_t;

typedef struct
{
    ha_cmd_type_t type;
    char entity_id[HA_MAX_ENTITY_ID_LEN];
    cJSON* data; // optional parameters
    char domain[32];
    char service[32];
    char method[8];
    char path[128];
} ha_cmd_t;

bool command_queue_init(void);
bool command_queue_receive(ha_cmd_t* out);
bool command_queue_enqueue(const ha_cmd_t* cmd);
bool command_queue_enqueue_light(const char* entity_id, const cJSON* params);
bool command_queue_enqueue_light_raw(const char* entity_id, cJSON* params);
bool command_queue_enqueue_light_brightness(const char* entity_id, uint8_t brightness);
bool command_queue_enqueue_light_level(const char* entity_id, uint8_t level);
bool command_queue_enqueue_light_color_temp(const char* entity_id, uint16_t mireds);
bool command_queue_enqueue_light_rgb(const char* entity_id, uint8_t r, uint8_t g, uint8_t b);
bool command_queue_enqueue_light_effect(const char* entity_id, const char* effect);
bool command_queue_enqueue_light_hs(const char* entity_id, float h, float s);
bool command_queue_enqueue_light_on(const char* entity_id);
bool command_queue_enqueue_light_off(const char* entity_id);
bool command_queue_enqueue_light_toggle(const char* entity_id, bool on);
bool command_queue_enqueue_scene(const char* entity_id);
bool command_queue_enqueue_automation(const char* entity_id);
bool command_queue_enqueue_service(const char* domain, const char* service, const cJSON* data);
bool command_queue_enqueue_service_domain(ha_domain_t domain, const char* service,
                                          const cJSON* data);
bool command_queue_enqueue_service_path(const char* path, const cJSON* data);
bool command_queue_enqueue_request(const char* method, const char* path, const cJSON* data);
bool command_queue_enqueue_post(const char* path, const cJSON* data);
bool command_queue_enqueue_put(const char* path, const cJSON* data);
bool command_queue_enqueue_get_state(const char* entity_id);
bool command_queue_enqueue_get_states(void);
size_t command_queue_pending(void);
bool command_queue_is_empty(void);
bool command_queue_initialized(void);
void command_queue_flush(void);
void command_queue_shutdown(void);
