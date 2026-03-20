#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "discovery.h"
#include "lvgl.h"

// -------------------------------------------------------------------------
// Status bar widget handles (defined in tabbed_ui.c, used by main.c)
// -------------------------------------------------------------------------

typedef struct
{
    lv_obj_t* status_bar;
    lv_obj_t* wifi_indicator;
    lv_obj_t* api_indicator;
    lv_obj_t* wifi_label;
    lv_obj_t* api_label;
} status_widgets_t;

extern status_widgets_t g_status;

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------

/**
 * Build the tileview-based tabbed UI from a discovery result.
 * Call this after the display and LVGL are initialized and g_discovery is populated.
 * Must NOT be called from within the LVGL task — acquires the LVGL mutex internally.
 */
void tabbed_ui_create(const disc_result_t* disc);

/**
 * Tear down the existing UI and rebuild it from a new discovery result.
 * Safe to call from any task — acquires LVGL mutex internally.
 */
void tabbed_ui_rebuild(const disc_result_t* disc);

/**
 * Returns true if the tabbed UI has been successfully initialized.
 */
bool tabbed_ui_ready(void);

// -------------------------------------------------------------------------
// Status functions (called from main.c / wifi/api event handlers)
// -------------------------------------------------------------------------

void update_status_indicators(bool wifi_connected, bool api_ok);
void tabbed_ui_set_offline(bool offline);

// -------------------------------------------------------------------------
// State update functions (called from ha_poll_task every 10s)
// All functions are thread-safe — they acquire the LVGL mutex internally.
// -------------------------------------------------------------------------

void tabbed_ui_update_light(size_t slot, bool on, uint8_t brightness);
void tabbed_ui_update_temperature(size_t slot, float temp);
void tabbed_ui_update_temperature_with_target(size_t slot, float temp, float target_temp);
void tabbed_ui_update_occupancy(size_t slot, bool occupied);
void tabbed_ui_update_media(size_t slot, const char* state, float volume, const char* media_title);
