#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

// Maximum field lengths
#define DISC_MAX_ENTITY_ID 64
#define DISC_MAX_NAME 64
#define DISC_MAX_AREA_ID 48
#define DISC_MAX_UNIT 16
#define DISC_MAX_DEVICE_CLASS 32
// Max total entities across all domains
#define DISC_MAX_ENTITIES 200
// Max entities per NVS key (fits within 4000B limit)
#define DISC_MAX_PER_DOMAIN 50

// Entity domain categories — each becomes one swipeable page
typedef enum
{
    DISC_DOMAIN_LIGHT = 0,   // light.*
    DISC_DOMAIN_TEMPERATURE, // sensor.* with temperature unit
    DISC_DOMAIN_CLIMATE,     // climate.* (thermostats)
    DISC_DOMAIN_OCCUPANCY,   // binary_sensor.* with occupancy/motion/presence class
    DISC_DOMAIN_SWITCH,      // switch.*, input_boolean.*
    DISC_DOMAIN_MEDIA,       // media_player.*
    DISC_DOMAIN_SCENE,       // scene.*
    DISC_DOMAIN_AUTOMATION,  // automation.*
    DISC_DOMAIN_COUNT        // sentinel — total number of domain types
} disc_domain_t;

// Single discovered entity
typedef struct
{
    char entity_id[DISC_MAX_ENTITY_ID];
    char friendly_name[DISC_MAX_NAME];
    char area_id[DISC_MAX_AREA_ID];           // empty string if not area-assigned
    char unit[DISC_MAX_UNIT];                 // unit_of_measurement (sensors only)
    char device_class[DISC_MAX_DEVICE_CLASS]; // HA device_class (binary_sensor only)
    disc_domain_t domain;
    bool enabled; // for automations: false if disabled in HA
} disc_entity_t;

// Full discovery result — heap-allocated entities array in PSRAM
typedef struct
{
    disc_entity_t* entities;                 // PSRAM allocation, may be NULL if empty
    size_t count;                            // total entity count across all domains
    size_t domain_counts[DISC_DOMAIN_COUNT]; // per-domain entity counts
    uint32_t checksum;                       // count-based cache validity check
    bool loaded_from_cache;                  // true if this result came from NVS
} disc_result_t;

// Area filter context built during discovery (freed after use)
typedef struct
{
    char target_area_id[DISC_MAX_AREA_ID];     // resolved area_id for the configured name
    char (*entity_area_ids)[DISC_MAX_AREA_ID]; // parallel array: entity_id → area_id
    char (*entity_ids_map)[DISC_MAX_ENTITY_ID];
    size_t map_count;
} disc_area_ctx_t;

// -------------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------------

/**
 * Run entity discovery against Home Assistant.
 *
 * Fetches /api/states (and optionally area/entity registries if area_filter
 * is non-empty), classifies entities by domain, and writes results to
 * out_result. Caller must call discovery_free() when done.
 *
 * @param area_filter  HA area name to filter by (e.g. "Office"), or NULL/"" for all
 * @param out_result   Output structure populated on ESP_OK
 * @return ESP_OK on success, ESP_ERR_* on failure
 */
esp_err_t discovery_run(const char* area_filter, disc_result_t* out_result);

/**
 * Free heap memory owned by a disc_result_t.
 * Safe to call on a zero-initialized or already-freed result.
 */
void discovery_free(disc_result_t* result);

/**
 * Save discovery result to NVS cache (one key per domain).
 * @return ESP_OK on success
 */
esp_err_t discovery_save_cache(const disc_result_t* result);

/**
 * Load discovery result from NVS cache into out_result.
 * Caller must call discovery_free() when done.
 * @return ESP_OK if cache exists and was loaded, ESP_ERR_NOT_FOUND if no cache
 */
esp_err_t discovery_load_cache(disc_result_t* out_result);

/**
 * Check whether a cached result is still valid.
 *
 * @param cached      Previously loaded cache result
 * @param fresh_count Entity count from a fresh HA poll
 * @param area_filter Current area filter setting
 * @return true if cache is still valid (no rebuild needed)
 */
bool discovery_cache_valid(const disc_result_t* cached, size_t fresh_count,
                           const char* area_filter);

/**
 * Return a human-readable label for a domain (e.g. "Lights", "Climate").
 */
const char* discovery_domain_label(disc_domain_t domain);

/**
 * Given a flat entity index i in disc_result_t.entities[], return its
 * slot index within its domain (i.e. how many same-domain entities come
 * before it). Used by ha_poll_task to map entity → widget slot.
 */
size_t discovery_entity_slot(const disc_result_t* result, size_t entity_index);
