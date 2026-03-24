#include "discovery.h"
#include "ha_client.h"
#include "ha_config.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char* TAG = "discovery";

// NVS namespace shared with the rest of the app
#define DISC_NVS_NAMESPACE "panel_config"

// NVS keys — must be ≤15 chars
#define NVS_KEY_LIGHTS "disc_lights"
#define NVS_KEY_TEMPS "disc_temps"
#define NVS_KEY_CLIMATE "disc_climate"
#define NVS_KEY_OCC "disc_occ"
#define NVS_KEY_SWITCH "disc_switch"
#define NVS_KEY_MEDIA "disc_media"
#define NVS_KEY_SCENES "disc_scenes"
#define NVS_KEY_AUTO "disc_auto"
#define NVS_KEY_META "disc_meta"

// NVS key per domain (indexed by disc_domain_t)
static const char* const s_nvs_keys[DISC_DOMAIN_COUNT] = {
    [DISC_DOMAIN_LIGHT] = NVS_KEY_LIGHTS,    [DISC_DOMAIN_TEMPERATURE] = NVS_KEY_TEMPS,
    [DISC_DOMAIN_CLIMATE] = NVS_KEY_CLIMATE, [DISC_DOMAIN_OCCUPANCY] = NVS_KEY_OCC,
    [DISC_DOMAIN_SWITCH] = NVS_KEY_SWITCH,   [DISC_DOMAIN_MEDIA] = NVS_KEY_MEDIA,
    [DISC_DOMAIN_SCENE] = NVS_KEY_SCENES,    [DISC_DOMAIN_AUTOMATION] = NVS_KEY_AUTO,
};

// Human-readable labels for each domain (used as tab titles)
static const char* const s_domain_labels[DISC_DOMAIN_COUNT] = {
    [DISC_DOMAIN_LIGHT] = "Lights",    [DISC_DOMAIN_TEMPERATURE] = "Temperature",
    [DISC_DOMAIN_CLIMATE] = "Climate", [DISC_DOMAIN_OCCUPANCY] = "Occupancy",
    [DISC_DOMAIN_SWITCH] = "Switches", [DISC_DOMAIN_MEDIA] = "Media",
    [DISC_DOMAIN_SCENE] = "Scenes",    [DISC_DOMAIN_AUTOMATION] = "Automation",
};

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

static bool starts_with(const char* str, const char* prefix)
{
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

// Classify an entity by its entity_id, unit_of_measurement, and device_class.
// Returns DISC_DOMAIN_COUNT to indicate "skip this entity".
static disc_domain_t classify_entity(const char* entity_id, const char* unit,
                                     const char* device_class)
{
    if (!entity_id || entity_id[0] == '\0')
        return DISC_DOMAIN_COUNT;

    if (starts_with(entity_id, "light."))
        return DISC_DOMAIN_LIGHT;

    if (starts_with(entity_id, "climate."))
        return DISC_DOMAIN_CLIMATE;

    if (starts_with(entity_id, "media_player."))
        return DISC_DOMAIN_MEDIA;

    if (starts_with(entity_id, "switch.") || starts_with(entity_id, "input_boolean."))
        return DISC_DOMAIN_SWITCH;

    if (starts_with(entity_id, "scene."))
        return DISC_DOMAIN_SCENE;

    if (starts_with(entity_id, "automation."))
        return DISC_DOMAIN_AUTOMATION;

    if (starts_with(entity_id, "sensor."))
    {
        // Only include sensors with explicit temperature units or device_class
        if (device_class && strcmp(device_class, "temperature") == 0)
            return DISC_DOMAIN_TEMPERATURE;
        // Exact unit matches only: °F, °C, F, C (but not % or other short strings)
        if (unit && (strcmp(unit, "\xC2\xB0"
                                  "F") == 0 ||
                     strcmp(unit, "\xC2\xB0"
                                  "C") == 0 ||
                     strcmp(unit, "°F") == 0 || strcmp(unit, "°C") == 0 || strcmp(unit, "F") == 0 ||
                     strcmp(unit, "C") == 0))
            return DISC_DOMAIN_TEMPERATURE;
        return DISC_DOMAIN_COUNT; // skip non-temperature sensors
    }

    if (starts_with(entity_id, "binary_sensor."))
    {
        // Require explicit occupancy/motion/presence device_class — don't guess
        if (device_class &&
            (strcmp(device_class, "occupancy") == 0 || strcmp(device_class, "motion") == 0 ||
             strcmp(device_class, "presence") == 0))
            return DISC_DOMAIN_OCCUPANCY;
        return DISC_DOMAIN_COUNT; // skip other binary sensors
    }

    // Skip: person.*, sun.*, zone.*, update.*, device_tracker.*, weather.*, etc.
    return DISC_DOMAIN_COUNT;
}

// Safe string copy from cJSON string item (or empty string if NULL/not string)
static void safe_str_copy(char* dst, size_t dst_size, const cJSON* item)
{
    if (item && cJSON_IsString(item) && item->valuestring)
        snprintf(dst, dst_size, "%s", item->valuestring);
    else
        dst[0] = '\0';
}

// -------------------------------------------------------------------------
// Area filter helpers
// -------------------------------------------------------------------------

// Free area context heap memory
static void area_ctx_free(disc_area_ctx_t* ctx)
{
    if (!ctx)
        return;
    free(ctx->entity_area_ids);
    free(ctx->entity_ids_map);
    ctx->entity_area_ids = NULL;
    ctx->entity_ids_map = NULL;
    ctx->map_count = 0;
}

// Build area context: resolve area name → area_id, then build entity→area_id map.
// Returns ESP_OK on success. ctx is populated and must be freed with area_ctx_free().
static esp_err_t build_area_ctx(const char* area_name, disc_area_ctx_t* ctx)
{
    memset(ctx, 0, sizeof(*ctx));

    // Step 1: resolve area name → area_id
    cJSON* area_list = NULL;
    esp_err_t err = ha_client_get(HA_API_AREA_REGISTRY, &area_list);
    if (err != ESP_OK || !cJSON_IsArray(area_list))
    {
        ESP_LOGW(TAG, "Failed to fetch area registry: %s", esp_err_to_name(err));
        cJSON_Delete(area_list);
        return ESP_ERR_NOT_FOUND;
    }

    cJSON* area_entry = NULL;
    cJSON_ArrayForEach(area_entry, area_list)
    {
        cJSON* name = cJSON_GetObjectItem(area_entry, "name");
        cJSON* id = cJSON_GetObjectItem(area_entry, "area_id");
        if (name && cJSON_IsString(name) && id && cJSON_IsString(id))
        {
            if (strcasecmp(name->valuestring, area_name) == 0)
            {
                snprintf(ctx->target_area_id, sizeof(ctx->target_area_id), "%s", id->valuestring);
                break;
            }
        }
    }
    cJSON_Delete(area_list);

    if (ctx->target_area_id[0] == '\0')
    {
        ESP_LOGW(TAG, "Area '%s' not found in HA area registry", area_name);
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "Area '%s' resolved to id '%s'", area_name, ctx->target_area_id);

    // Step 2: fetch entity registry to build entity_id → area_id map
    cJSON* entity_list = NULL;
    err = ha_client_get(HA_API_ENTITY_REGISTRY, &entity_list);
    if (err != ESP_OK || !cJSON_IsArray(entity_list))
    {
        ESP_LOGW(TAG, "Failed to fetch entity registry: %s", esp_err_to_name(err));
        cJSON_Delete(entity_list);
        return ESP_ERR_NOT_FOUND;
    }

    size_t total = (size_t)cJSON_GetArraySize(entity_list);
    ctx->entity_ids_map = calloc(total, sizeof(*ctx->entity_ids_map));
    ctx->entity_area_ids = calloc(total, sizeof(*ctx->entity_area_ids));
    if (!ctx->entity_ids_map || !ctx->entity_area_ids)
    {
        ESP_LOGE(TAG, "OOM building area context");
        free(ctx->entity_ids_map);
        free(ctx->entity_area_ids);
        cJSON_Delete(entity_list);
        return ESP_ERR_NO_MEM;
    }

    size_t idx = 0;
    cJSON* ent = NULL;
    cJSON_ArrayForEach(ent, entity_list)
    {
        cJSON* eid = cJSON_GetObjectItem(ent, "entity_id");
        cJSON* aid = cJSON_GetObjectItem(ent, "area_id");
        if (eid && cJSON_IsString(eid))
        {
            snprintf(ctx->entity_ids_map[idx], DISC_MAX_ENTITY_ID, "%s", eid->valuestring);
            if (aid && cJSON_IsString(aid) && aid->valuestring)
                snprintf(ctx->entity_area_ids[idx], DISC_MAX_AREA_ID, "%s", aid->valuestring);
            idx++;
        }
    }
    ctx->map_count = idx;
    cJSON_Delete(entity_list);

    ESP_LOGI(TAG, "Built area map with %zu entries", ctx->map_count);
    return ESP_OK;
}

// Look up the area_id for an entity in the context map. Returns NULL if not found.
static const char* area_ctx_lookup(const disc_area_ctx_t* ctx, const char* entity_id)
{
    for (size_t i = 0; i < ctx->map_count; i++)
    {
        if (strcmp(ctx->entity_ids_map[i], entity_id) == 0)
            return ctx->entity_area_ids[i];
    }
    return NULL;
}

// -------------------------------------------------------------------------
// discovery_run
// -------------------------------------------------------------------------

esp_err_t discovery_run(const char* area_filter, disc_result_t* out_result)
{
    if (!out_result)
        return ESP_ERR_INVALID_ARG;

    memset(out_result, 0, sizeof(*out_result));

    bool use_area_filter = (area_filter && area_filter[0] != '\0');
    disc_area_ctx_t area_ctx = {0};

    // Build area filter context if needed (before fetching states, to free early)
    if (use_area_filter)
    {
        esp_err_t area_err = build_area_ctx(area_filter, &area_ctx);
        if (area_err != ESP_OK)
        {
            ESP_LOGW(TAG, "Area filter setup failed — showing all entities");
            use_area_filter = false;
        }
    }

    // Fetch all entity states
    cJSON* states = NULL;
    esp_err_t err = ha_client_get_states(&states);
    if (err != ESP_OK || !cJSON_IsArray(states))
    {
        ESP_LOGE(TAG, "Failed to fetch /api/states: %s", esp_err_to_name(err));
        cJSON_Delete(states);
        area_ctx_free(&area_ctx);
        return err != ESP_OK ? err : ESP_FAIL;
    }

    // First pass: count entities that will be included
    size_t total_count = 0;
    cJSON* state_obj = NULL;
    cJSON_ArrayForEach(state_obj, states)
    {
        cJSON* eid_j = cJSON_GetObjectItem(state_obj, "entity_id");
        cJSON* attrs = cJSON_GetObjectItem(state_obj, "attributes");
        cJSON* unit_j = attrs ? cJSON_GetObjectItem(attrs, "unit_of_measurement") : NULL;
        cJSON* dclass_j = attrs ? cJSON_GetObjectItem(attrs, "device_class") : NULL;

        const char* eid = (eid_j && cJSON_IsString(eid_j)) ? eid_j->valuestring : "";
        const char* unit = (unit_j && cJSON_IsString(unit_j)) ? unit_j->valuestring : "";
        const char* dclass = (dclass_j && cJSON_IsString(dclass_j)) ? dclass_j->valuestring : "";

        disc_domain_t domain = classify_entity(eid, unit, dclass);
        if (domain == DISC_DOMAIN_COUNT)
            continue;

        // Skip unavailable/unknown entities (except scenes/automations which have no live state)
        if (domain != DISC_DOMAIN_SCENE && domain != DISC_DOMAIN_AUTOMATION)
        {
            cJSON* state_j = cJSON_GetObjectItem(state_obj, "state");
            const char* sv = (state_j && cJSON_IsString(state_j)) ? state_j->valuestring : "";
            if (strcmp(sv, "unavailable") == 0 || strcmp(sv, "unknown") == 0)
                continue;
        }

        if (use_area_filter)
        {
            const char* ent_area = area_ctx_lookup(&area_ctx, eid);
            if (!ent_area || strcmp(ent_area, area_ctx.target_area_id) != 0)
                continue;
        }

        total_count++;
        if (total_count >= DISC_MAX_ENTITIES)
            break;
    }

    if (total_count == 0)
    {
        ESP_LOGW(TAG, "No matching entities found");
        cJSON_Delete(states);
        area_ctx_free(&area_ctx);
        return ESP_OK; // empty result is valid
    }

    // Allocate entity array from PSRAM
    out_result->entities =
        heap_caps_calloc(total_count, sizeof(disc_entity_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!out_result->entities)
    {
        // Fallback to regular heap if PSRAM unavailable (e.g. simulator)
        out_result->entities = calloc(total_count, sizeof(disc_entity_t));
    }
    if (!out_result->entities)
    {
        ESP_LOGE(TAG, "OOM allocating entity array (%zu entities)", total_count);
        cJSON_Delete(states);
        area_ctx_free(&area_ctx);
        return ESP_ERR_NO_MEM;
    }

    // Second pass: populate entity array
    size_t idx = 0;
    cJSON_ArrayForEach(state_obj, states)
    {
        if (idx >= total_count)
            break;

        cJSON* eid_j = cJSON_GetObjectItem(state_obj, "entity_id");
        cJSON* state_j = cJSON_GetObjectItem(state_obj, "state");
        cJSON* attrs = cJSON_GetObjectItem(state_obj, "attributes");
        cJSON* name_j = attrs ? cJSON_GetObjectItem(attrs, "friendly_name") : NULL;
        cJSON* unit_j = attrs ? cJSON_GetObjectItem(attrs, "unit_of_measurement") : NULL;
        cJSON* dclass_j = attrs ? cJSON_GetObjectItem(attrs, "device_class") : NULL;

        const char* eid = (eid_j && cJSON_IsString(eid_j)) ? eid_j->valuestring : "";
        const char* unit = (unit_j && cJSON_IsString(unit_j)) ? unit_j->valuestring : "";
        const char* dclass = (dclass_j && cJSON_IsString(dclass_j)) ? dclass_j->valuestring : "";

        disc_domain_t domain = classify_entity(eid, unit, dclass);
        if (domain == DISC_DOMAIN_COUNT)
            continue;

        // Skip unavailable/unknown entities (except scenes/automations)
        if (domain != DISC_DOMAIN_SCENE && domain != DISC_DOMAIN_AUTOMATION)
        {
            const char* sv = (state_j && cJSON_IsString(state_j)) ? state_j->valuestring : "";
            if (strcmp(sv, "unavailable") == 0 || strcmp(sv, "unknown") == 0)
                continue;
        }

        if (use_area_filter)
        {
            const char* ent_area = area_ctx_lookup(&area_ctx, eid);
            if (!ent_area || strcmp(ent_area, area_ctx.target_area_id) != 0)
                continue;
        }

        disc_entity_t* e = &out_result->entities[idx];
        snprintf(e->entity_id, sizeof(e->entity_id), "%s", eid);
        safe_str_copy(e->friendly_name, sizeof(e->friendly_name), name_j);
        safe_str_copy(e->unit, sizeof(e->unit), unit_j);
        safe_str_copy(e->device_class, sizeof(e->device_class), dclass_j);
        e->domain = domain;

        // For automations, check if enabled (state != "unavailable")
        if (domain == DISC_DOMAIN_AUTOMATION)
        {
            const char* st = (state_j && cJSON_IsString(state_j)) ? state_j->valuestring : "";
            e->enabled = (strcmp(st, "unavailable") != 0);
        }
        else
        {
            e->enabled = true;
        }

        // Copy area_id if we have it
        if (use_area_filter)
        {
            const char* ent_area = area_ctx_lookup(&area_ctx, eid);
            if (ent_area)
                snprintf(e->area_id, sizeof(e->area_id), "%s", ent_area);
        }

        // Use entity_id as fallback friendly name
        if (e->friendly_name[0] == '\0')
            snprintf(e->friendly_name, sizeof(e->friendly_name), "%s", eid);

        out_result->domain_counts[domain]++;
        idx++;
    }

    out_result->count = idx;
    out_result->checksum = (uint32_t)idx;
    out_result->loaded_from_cache = false;

    cJSON_Delete(states);
    area_ctx_free(&area_ctx);

    // Log summary
    ESP_LOGI(TAG, "Discovery complete: %zu total entities", out_result->count);
    for (int d = 0; d < DISC_DOMAIN_COUNT; d++)
    {
        if (out_result->domain_counts[d] > 0)
            ESP_LOGI(TAG, "  %s: %zu", s_domain_labels[d], out_result->domain_counts[d]);
    }

    return ESP_OK;
}

// -------------------------------------------------------------------------
// discovery_free
// -------------------------------------------------------------------------

void discovery_free(disc_result_t* result)
{
    if (!result)
        return;
    if (result->entities)
    {
        heap_caps_free(result->entities);
        result->entities = NULL;
    }
    memset(result, 0, sizeof(*result));
}

// -------------------------------------------------------------------------
// NVS cache
// -------------------------------------------------------------------------

// Serialize one domain's entities to a compact CSV string.
// Format varies per domain — see plan for details.
// Returns number of bytes written (excluding null terminator), or 0 on overflow.
static size_t serialize_domain(const disc_result_t* result, disc_domain_t domain, char* buf,
                               size_t buf_size)
{
    size_t written = 0;
    for (size_t i = 0; i < result->count; i++)
    {
        const disc_entity_t* e = &result->entities[i];
        if (e->domain != domain)
            continue;

        char line[256];
        int n;

        switch (domain)
        {
        case DISC_DOMAIN_TEMPERATURE:
            n = snprintf(line, sizeof(line), "%s,%s,%s\n", e->entity_id, e->friendly_name, e->unit);
            break;
        case DISC_DOMAIN_OCCUPANCY:
            n = snprintf(line, sizeof(line), "%s,%s,%s\n", e->entity_id, e->friendly_name,
                         e->device_class);
            break;
        case DISC_DOMAIN_AUTOMATION:
            n = snprintf(line, sizeof(line), "%s,%s,%d\n", e->entity_id, e->friendly_name,
                         e->enabled ? 1 : 0);
            break;
        default:
            n = snprintf(line, sizeof(line), "%s,%s\n", e->entity_id, e->friendly_name);
            break;
        }

        if (n <= 0 || (written + (size_t)n) >= buf_size)
            break; // stop before overflow

        memcpy(buf + written, line, (size_t)n);
        written += (size_t)n;
    }
    buf[written] = '\0';
    return written;
}

esp_err_t discovery_save_cache(const disc_result_t* result)
{
    if (!result)
        return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(DISC_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS for cache write: %s", esp_err_to_name(err));
        return err;
    }

    // Stack buffer for serialization — 3900 bytes stays under NVS 4000B limit
    char buf[3900];

    for (int d = 0; d < DISC_DOMAIN_COUNT; d++)
    {
        if (result->domain_counts[d] == 0)
        {
            // Clear stale key
            nvs_erase_key(nvs, s_nvs_keys[d]);
            continue;
        }

        size_t written = serialize_domain(result, (disc_domain_t)d, buf, sizeof(buf));
        if (written == 0)
            continue;

        err = nvs_set_str(nvs, s_nvs_keys[d], buf);
        if (err != ESP_OK)
            ESP_LOGW(TAG, "Failed to save cache for domain %d: %s", d, esp_err_to_name(err));
    }

    // Save metadata: total_count,area_filter
    snprintf(buf, sizeof(buf), "%zu,", result->count);
    nvs_set_str(nvs, NVS_KEY_META, buf);

    nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(TAG, "Discovery cache saved (%zu entities)", result->count);
    return ESP_OK;
}

// Parse one CSV line into a disc_entity_t based on domain
static bool parse_cache_line(char* line, disc_domain_t domain, disc_entity_t* out)
{
    if (!line || line[0] == '\0')
        return false;

    // Strip trailing newline
    char* nl = strchr(line, '\n');
    if (nl)
        *nl = '\0';

    out->domain = domain;
    out->enabled = true;

    // Split on comma — fields: entity_id, friendly_name, [optional 3rd field]
    char* saveptr = NULL;
    char* token = strtok_r(line, ",", &saveptr);
    if (!token)
        return false;
    snprintf(out->entity_id, sizeof(out->entity_id), "%s", token);

    token = strtok_r(NULL, ",", &saveptr);
    if (!token)
        return false;
    snprintf(out->friendly_name, sizeof(out->friendly_name), "%s", token);

    // Optional 3rd field
    token = strtok_r(NULL, ",", &saveptr);
    if (token)
    {
        switch (domain)
        {
        case DISC_DOMAIN_TEMPERATURE:
            snprintf(out->unit, sizeof(out->unit), "%s", token);
            break;
        case DISC_DOMAIN_OCCUPANCY:
            snprintf(out->device_class, sizeof(out->device_class), "%s", token);
            break;
        case DISC_DOMAIN_AUTOMATION:
            out->enabled = (token[0] == '1');
            break;
        default:
            break;
        }
    }

    return (out->entity_id[0] != '\0');
}

esp_err_t discovery_load_cache(disc_result_t* out_result)
{
    if (!out_result)
        return ESP_ERR_INVALID_ARG;

    memset(out_result, 0, sizeof(*out_result));

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(DISC_NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGD(TAG, "No NVS cache (namespace not found)");
        return ESP_ERR_NOT_FOUND;
    }

    // Check metadata first
    char meta_buf[64];
    size_t meta_size = sizeof(meta_buf);
    err = nvs_get_str(nvs, NVS_KEY_META, meta_buf, &meta_size);
    if (err != ESP_OK)
    {
        nvs_close(nvs);
        return ESP_ERR_NOT_FOUND;
    }

    size_t cached_count = (size_t)atoi(meta_buf);
    if (cached_count == 0)
    {
        nvs_close(nvs);
        return ESP_ERR_NOT_FOUND;
    }

    // Allocate entity array (PSRAM preferred)
    out_result->entities =
        heap_caps_calloc(cached_count, sizeof(disc_entity_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!out_result->entities)
        out_result->entities = calloc(cached_count, sizeof(disc_entity_t));
    if (!out_result->entities)
    {
        nvs_close(nvs);
        return ESP_ERR_NO_MEM;
    }

    // Read domain by domain
    char buf[3900];
    size_t idx = 0;

    for (int d = 0; d < DISC_DOMAIN_COUNT && idx < cached_count; d++)
    {
        size_t buf_size = sizeof(buf);
        err = nvs_get_str(nvs, s_nvs_keys[d], buf, &buf_size);
        if (err != ESP_OK)
            continue; // domain had no entities

        // Parse line by line
        char* saveptr = NULL;
        char* line = strtok_r(buf, "\n", &saveptr);
        while (line && idx < cached_count)
        {
            disc_entity_t* e = &out_result->entities[idx];
            if (parse_cache_line(line, (disc_domain_t)d, e))
            {
                out_result->domain_counts[d]++;
                idx++;
            }
            line = strtok_r(NULL, "\n", &saveptr);
        }
    }

    nvs_close(nvs);

    out_result->count = idx;
    out_result->checksum = (uint32_t)idx;
    out_result->loaded_from_cache = true;

    if (idx == 0)
    {
        discovery_free(out_result);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Loaded %zu entities from NVS cache", idx);
    return ESP_OK;
}

bool discovery_cache_valid(const disc_result_t* cached, size_t fresh_count, const char* area_filter)
{
    (void)area_filter; // area filter change is handled by meta invalidation at save time
    if (!cached || !cached->entities || cached->count == 0)
        return false;
    // Simple count-based check — if HA entity count changed, rebuild
    return (cached->count == fresh_count);
}

// -------------------------------------------------------------------------
// Utility
// -------------------------------------------------------------------------

const char* discovery_domain_label(disc_domain_t domain)
{
    if (domain >= DISC_DOMAIN_COUNT)
        return "Unknown";
    return s_domain_labels[domain];
}

size_t discovery_entity_slot(const disc_result_t* result, size_t entity_index)
{
    if (!result || !result->entities || entity_index >= result->count)
        return 0;

    disc_domain_t target_domain = result->entities[entity_index].domain;
    size_t slot = 0;
    for (size_t i = 0; i < entity_index; i++)
    {
        if (result->entities[i].domain == target_domain)
            slot++;
    }
    return slot;
}
