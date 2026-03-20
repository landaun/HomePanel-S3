#include "scene_service.h"
#include "command_queue.h"
#include "ha_client.h"
#include "cJSON.h"
#include "nvs.h"
#include <string.h>

#define MAX_FAVORITES 8
static char s_favorites[MAX_FAVORITES][HA_MAX_ENTITY_ID_LEN];
static size_t s_fav_count;

#define FAV_NS "scene"
#define FAV_KEY "favs"

static void load_favorites(void)
{
    nvs_handle_t h;
    size_t required = 0;
    if (nvs_open(FAV_NS, NVS_READONLY, &h) == ESP_OK)
    {
        if (nvs_get_blob(h, FAV_KEY, NULL, &required) == ESP_OK && required <= sizeof(s_favorites))
        {
            if (nvs_get_blob(h, FAV_KEY, s_favorites, &required) == ESP_OK)
            {
                s_fav_count = required / sizeof(s_favorites[0]);
            }
        }
        nvs_close(h);
    }
}

static void save_favorites(void)
{
    nvs_handle_t h;
    if (nvs_open(FAV_NS, NVS_READWRITE, &h) == ESP_OK)
    {
        size_t size = s_fav_count * sizeof(s_favorites[0]);
        nvs_set_blob(h, FAV_KEY, s_favorites, size);
        nvs_commit(h);
        nvs_close(h);
    }
}

bool scene_service_is_favorite(const char* entity_id)
{
    if (!entity_id)
    {
        return false;
    }
    for (size_t i = 0; i < s_fav_count; ++i)
    {
        if (strcmp(s_favorites[i], entity_id) == 0)
        {
            return true;
        }
    }
    return false;
}

bool scene_service_toggle_favorite(const char* entity_id)
{
    if (!entity_id)
    {
        return false;
    }
    for (size_t i = 0; i < s_fav_count; ++i)
    {
        if (strcmp(s_favorites[i], entity_id) == 0)
        {
            memmove(&s_favorites[i], &s_favorites[i + 1],
                    (s_fav_count - i - 1) * sizeof(s_favorites[0]));
            s_fav_count--;
            save_favorites();
            return false;
        }
    }
    if (s_fav_count >= MAX_FAVORITES)
    {
        return false;
    }
    strncpy(s_favorites[s_fav_count], entity_id, HA_MAX_ENTITY_ID_LEN - 1);
    s_favorites[s_fav_count][HA_MAX_ENTITY_ID_LEN - 1] = '\0';
    s_fav_count++;
    save_favorites();
    return true;
}

int scene_service_list_favorites(char favorites[][HA_MAX_ENTITY_ID_LEN], size_t max_favorites)
{
    if (!favorites || max_favorites == 0)
    {
        return 0;
    }
    size_t count = s_fav_count < max_favorites ? s_fav_count : max_favorites;
    for (size_t i = 0; i < count; ++i)
    {
        strncpy(favorites[i], s_favorites[i], HA_MAX_ENTITY_ID_LEN);
        favorites[i][HA_MAX_ENTITY_ID_LEN - 1] = '\0';
    }
    return (int)count;
}

void scene_service_init(void) { load_favorites(); }

bool scene_service_trigger_scene(const char* entity_id)
{
    return command_queue_enqueue_scene(entity_id);
}

bool scene_service_trigger_automation(const char* entity_id)
{
    return command_queue_enqueue_automation(entity_id);
}

int scene_service_list_scenes(scene_t* scenes, size_t max_scenes)
{
    if (!scenes || max_scenes == 0)
    {
        return 0;
    }

    cJSON* states = NULL;
    if (ha_client_get_states(&states) != ESP_OK || !cJSON_IsArray(states))
    {
        if (states)
        {
            cJSON_Delete(states);
        }
        return 0;
    }

    size_t count = 0;
    cJSON* item = NULL;
    cJSON_ArrayForEach(item, states)
    {
        cJSON* id = cJSON_GetObjectItem(item, "entity_id");
        if (!cJSON_IsString(id) || strncmp(id->valuestring, "scene.", 6) != 0)
        {
            continue;
        }
        if (count >= max_scenes)
        {
            break;
        }
        strncpy(scenes[count].entity_id, id->valuestring, HA_MAX_ENTITY_ID_LEN - 1);
        scenes[count].entity_id[HA_MAX_ENTITY_ID_LEN - 1] = '\0';

        cJSON* attr = cJSON_GetObjectItem(item, "attributes");
        const char* name = NULL;
        if (cJSON_IsObject(attr))
        {
            cJSON* friendly = cJSON_GetObjectItem(attr, "friendly_name");
            if (cJSON_IsString(friendly))
            {
                name = friendly->valuestring;
            }
        }
        if (name)
        {
            strncpy(scenes[count].name, name, sizeof(scenes[count].name) - 1);
            scenes[count].name[sizeof(scenes[count].name) - 1] = '\0';
        }
        else
        {
            scenes[count].name[0] = '\0';
        }
        count++;
    }
    cJSON_Delete(states);
    return count;
}

int scene_service_list_automations(automation_t* automations, size_t max_automations)
{
    if (!automations || max_automations == 0)
    {
        return 0;
    }

    cJSON* states = NULL;
    if (ha_client_get_states(&states) != ESP_OK || !cJSON_IsArray(states))
    {
        if (states)
        {
            cJSON_Delete(states);
        }
        return 0;
    }

    size_t count = 0;
    cJSON* item = NULL;
    cJSON_ArrayForEach(item, states)
    {
        cJSON* id = cJSON_GetObjectItem(item, "entity_id");
        if (!cJSON_IsString(id) || strncmp(id->valuestring, "automation.", 11) != 0)
        {
            continue;
        }
        if (count >= max_automations)
        {
            break;
        }

        strncpy(automations[count].entity_id, id->valuestring, HA_MAX_ENTITY_ID_LEN - 1);
        automations[count].entity_id[HA_MAX_ENTITY_ID_LEN - 1] = '\0';

        cJSON* attr = cJSON_GetObjectItem(item, "attributes");
        const char* name = NULL;
        if (cJSON_IsObject(attr))
        {
            cJSON* friendly = cJSON_GetObjectItem(attr, "friendly_name");
            if (cJSON_IsString(friendly))
            {
                name = friendly->valuestring;
            }
        }
        if (name)
        {
            strncpy(automations[count].name, name, sizeof(automations[count].name) - 1);
            automations[count].name[sizeof(automations[count].name) - 1] = '\0';
        }
        else
        {
            automations[count].name[0] = '\0';
        }

        cJSON* state_item = cJSON_GetObjectItem(item, "state");
        automations[count].enabled =
            cJSON_IsString(state_item) && strcmp(state_item->valuestring, "on") == 0;
        count++;
    }

    cJSON_Delete(states);
    return count;
}
