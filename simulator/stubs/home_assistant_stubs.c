#include "command_queue.h"
#include "ha_config.h"
#include "scene_service.h"

#include "esp_log.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static const char* TAG = "sim_ha";

static const char* k_favorite_candidates[] = {
    "scene.good_morning",
    "scene.good_night",
};
static bool k_favorite_enabled[] = {true, false};

// ---------------------------------------------------------------------------
// command_queue stubs — no real HA connection in the simulator
// ---------------------------------------------------------------------------

bool command_queue_init(void) { return true; }
bool command_queue_receive(ha_cmd_t* out) { (void)out; return false; }
bool command_queue_enqueue(const ha_cmd_t* cmd) { (void)cmd; return true; }
bool command_queue_enqueue_light(const char* e, const cJSON* p) { (void)e; (void)p; return true; }
bool command_queue_enqueue_light_raw(const char* e, cJSON* p) { (void)e; cJSON_Delete(p); return true; }
bool command_queue_enqueue_light_brightness(const char* e, uint8_t b) { (void)e; (void)b; return true; }
bool command_queue_enqueue_light_level(const char* e, uint8_t l) { (void)e; (void)l; return true; }
bool command_queue_enqueue_light_color_temp(const char* e, uint16_t m) { (void)e; (void)m; return true; }
bool command_queue_enqueue_light_rgb(const char* e, uint8_t r, uint8_t g, uint8_t b) { (void)e; (void)r; (void)g; (void)b; return true; }
bool command_queue_enqueue_light_effect(const char* e, const char* f) { (void)e; (void)f; return true; }
bool command_queue_enqueue_light_hs(const char* e, float h, float s) { (void)e; (void)h; (void)s; return true; }
bool command_queue_enqueue_light_on(const char* e) { (void)e; return true; }
bool command_queue_enqueue_light_off(const char* e) { (void)e; return true; }
bool command_queue_enqueue_light_toggle(const char* e, bool on) { (void)e; (void)on; return true; }
bool command_queue_enqueue_scene(const char* e) { (void)e; return true; }
bool command_queue_enqueue_automation(const char* e) { (void)e; return true; }
bool command_queue_enqueue_service(const char* d, const char* s, const cJSON* data) { (void)d; (void)s; (void)data; return true; }
bool command_queue_enqueue_service_domain(ha_domain_t d, const char* s, const cJSON* data) { (void)d; (void)s; (void)data; return true; }
bool command_queue_enqueue_service_path(const char* p, const cJSON* data) { (void)p; (void)data; return true; }
bool command_queue_enqueue_request(const char* m, const char* p, const cJSON* data) { (void)m; (void)p; (void)data; return true; }
bool command_queue_enqueue_post(const char* p, const cJSON* data) { (void)p; (void)data; return true; }
bool command_queue_enqueue_put(const char* p, const cJSON* data) { (void)p; (void)data; return true; }
bool command_queue_enqueue_get_state(const char* e) { (void)e; return true; }
bool command_queue_enqueue_get_states(void) { return true; }
size_t command_queue_pending(void) { return 0; }
bool command_queue_is_empty(void) { return true; }
bool command_queue_initialized(void) { return true; }
void command_queue_flush(void) {}
void command_queue_shutdown(void) {}

// ---------------------------------------------------------------------------
// scene_service stubs
// ---------------------------------------------------------------------------

void scene_service_init(void)
{
}

bool scene_service_trigger_scene(const char* entity_id)
{
    ESP_LOGI(TAG, "Trigger scene %s", entity_id);
    return true;
}

bool scene_service_trigger_automation(const char* entity_id)
{
    ESP_LOGI(TAG, "Trigger automation %s", entity_id);
    return true;
}

bool scene_service_toggle_favorite(const char* entity_id)
{
    for (size_t i = 0; i < sizeof(k_favorite_candidates) / sizeof(k_favorite_candidates[0]); ++i)
    {
        if (strcmp(k_favorite_candidates[i], entity_id) == 0)
        {
            k_favorite_enabled[i] = !k_favorite_enabled[i];
            return true;
        }
    }
    return false;
}

bool scene_service_is_favorite(const char* entity_id)
{
    for (size_t i = 0; i < sizeof(k_favorite_candidates) / sizeof(k_favorite_candidates[0]); ++i)
    {
        if (strcmp(k_favorite_candidates[i], entity_id) == 0)
        {
            return k_favorite_enabled[i];
        }
    }
    return false;
}

int scene_service_list_favorites(char favorites[][HA_MAX_ENTITY_ID_LEN], size_t max_favorites)
{
    size_t out = 0;
    size_t total = sizeof(k_favorite_candidates) / sizeof(k_favorite_candidates[0]);
    for (size_t i = 0; i < total && out < max_favorites; ++i)
    {
        if (!k_favorite_enabled[i]) continue;
        strncpy(favorites[out], k_favorite_candidates[i], HA_MAX_ENTITY_ID_LEN - 1);
        favorites[out][HA_MAX_ENTITY_ID_LEN - 1] = '\0';
        ++out;
    }
    return (int)out;
}
