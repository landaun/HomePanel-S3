#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "ha_config.h"

typedef struct
{
    char entity_id[HA_MAX_ENTITY_ID_LEN];
    char name[64];
} scene_t;

typedef struct
{
    char entity_id[HA_MAX_ENTITY_ID_LEN];
    char name[64];
    bool enabled;
} automation_t;

// Initialize scene service (placeholder for future enhancements)
void scene_service_init(void);

// Enqueue scene activation for a scene entity
bool scene_service_trigger_scene(const char* entity_id);

// Enqueue automation trigger for an automation entity
bool scene_service_trigger_automation(const char* entity_id);

// Toggle favorite state for a scene or automation
bool scene_service_toggle_favorite(const char* entity_id);

// Check if an entity is marked as favorite
bool scene_service_is_favorite(const char* entity_id);

// List current favorites; returns count copied
int scene_service_list_favorites(char favorites[][HA_MAX_ENTITY_ID_LEN], size_t max_favorites);

// Fetch lists of scenes or automations from Home Assistant
int scene_service_list_scenes(scene_t* scenes, size_t max_scenes);
int scene_service_list_automations(automation_t* automations, size_t max_automations);
