#include "tabbed_ui.h"
#include "discovery.h"
#include "command_queue.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lvgl_port.h"
#include "scene_service.h"
#include "ui_icons.h"
#include "ui_style.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char* TAG = "tabbed_ui";

// -------------------------------------------------------------------------
// Constants
// -------------------------------------------------------------------------

#define STATUS_BAR_H 30
#define TAB_HEADER_H 44
#define SCREEN_W 800
#define SCREEN_H 480
#define TILEVIEW_Y (STATUS_BAR_H + TAB_HEADER_H)
#define TILEVIEW_H (SCREEN_H - TILEVIEW_Y)

// Tab button highlight colors
#define TAB_ACTIVE_COLOR UI_COLOR_ACCENT_TEAL
#define TAB_INACTIVE_COLOR UI_COLOR_SECONDARY_TEXT

// -------------------------------------------------------------------------
// Per-slot widget struct (one entry per discovered entity in a domain)
// -------------------------------------------------------------------------

typedef struct
{
    char entity_id[64];
    char name[64];
    lv_obj_t* label;
    lv_obj_t* toggle;
    lv_obj_t* slider;
    lv_obj_t* play_btn;
    bool has_value;
    bool is_on;
    uint8_t brightness;
    float temp;
    float target_temp;
    bool occupied;
    bool enabled; // automations
} tab_widget_t;

// -------------------------------------------------------------------------
// Module state
// -------------------------------------------------------------------------

static lv_obj_t* s_tab_header = NULL;
static lv_obj_t* s_tileview = NULL;
static lv_obj_t* s_offline_badge = NULL;
static bool s_initialized = false;

status_widgets_t g_status = {0};

// Per-domain widget arrays (PSRAM)
static tab_widget_t* s_widgets[DISC_DOMAIN_COUNT] = {0};
static size_t s_widget_counts[DISC_DOMAIN_COUNT] = {0};

// Tab button objects — one per active page (max DISC_DOMAIN_COUNT pages)
static lv_obj_t* s_tab_buttons[DISC_DOMAIN_COUNT] = {0};
static int s_tab_domain[DISC_DOMAIN_COUNT] = {0}; // domain for each tab index
static size_t s_tab_count = 0;

// (no modal state — climate controls are inline on each card)

// -------------------------------------------------------------------------
// Forward declarations
// -------------------------------------------------------------------------

static void build_page(lv_obj_t* tile, const disc_result_t* disc, disc_domain_t domain,
                       size_t domain_idx);
static void update_tab_highlight(size_t active_tab);
static void tileview_changed_cb(lv_event_t* e);
static void tab_btn_clicked_cb(lv_event_t* e);

// Light callbacks
static void light_switch_cb(lv_event_t* e);
static void light_slider_cb(lv_event_t* e);

// Climate callbacks
static void climate_temp_btn_cb(lv_event_t* e);

// Media callbacks
static void media_play_pause_cb(lv_event_t* e);
static void media_volume_cb(lv_event_t* e);

// Scene/automation callbacks
static void scene_btn_cb(lv_event_t* e);
static void automation_btn_cb(lv_event_t* e);
static void btn_delete_cb(lv_event_t* e);

// -------------------------------------------------------------------------
// Domain tab icons and accent colors
// -------------------------------------------------------------------------

// -------------------------------------------------------------------------
// Helper: room icon for lights
// -------------------------------------------------------------------------

static const char* get_room_icon(const char* name)
{
    if (!name)
        return ICON_LIGHTBULB;
    char lower[64];
    size_t i;
    for (i = 0; name[i] && i < sizeof(lower) - 1; i++)
        lower[i] = (name[i] >= 'A' && name[i] <= 'Z') ? name[i] + 32 : name[i];
    lower[i] = '\0';

    if (strstr(lower, "office"))
        return ICON_BRIEFCASE;
    if (strstr(lower, "dining"))
        return ICON_UTENSILS;
    if (strstr(lower, "fire"))
        return ICON_FIRE;
    return ICON_LIGHTBULB;
}

// -------------------------------------------------------------------------
// Name-based entity filters (configured via idf.py menuconfig)
// -------------------------------------------------------------------------

// Returns true if the name ends with a digit (optionally with trailing spaces)
static bool name_ends_with_number(const char* name)
{
    if (!name || name[0] == '\0')
        return false;
    size_t len = strlen(name);
    while (len > 0 && name[len - 1] == ' ')
        len--;
    return (len > 0 && name[len - 1] >= '0' && name[len - 1] <= '9');
}

// Case-insensitive substring search
static bool name_contains_ci(const char* name, const char* needle)
{
    if (!name || !needle || needle[0] == '\0')
        return false;
    size_t nl = strlen(needle);
    size_t sl = strlen(name);
    if (nl > sl)
        return false;
    for (size_t i = 0; i <= sl - nl; i++)
    {
        size_t j;
        for (j = 0; j < nl; j++)
        {
            char nc = name[i + j];
            char hc = needle[j];
            if (nc >= 'A' && nc <= 'Z')
                nc += 32;
            if (hc >= 'A' && hc <= 'Z')
                hc += 32;
            if (nc != hc)
                break;
        }
        if (j == nl)
            return true;
    }
    return false;
}

// Check a name against a compile-time comma-separated keyword list.
// keywords_csv is a string literal like "desk,rack,strip" from Kconfig.
static bool name_matches_csv(const char* name, const char* keywords_csv)
{
    if (!keywords_csv || keywords_csv[0] == '\0')
        return false;
    // Work on a stack copy to avoid modifying the literal
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", keywords_csv);
    char* saveptr = NULL;
    char* tok = strtok_r(buf, ",", &saveptr);
    while (tok)
    {
        // Trim leading spaces from token
        while (*tok == ' ')
            tok++;
        if (*tok && name_contains_ci(name, tok))
            return true;
        tok = strtok_r(NULL, ",", &saveptr);
    }
    return false;
}

static bool light_should_skip(const char* name)
{
    if (CONFIG_HA_ENTITY_SKIP_NUMBERED && name_ends_with_number(name))
        return true;
    return name_matches_csv(name, CONFIG_HA_ENTITY_SKIP_LIGHT_KEYWORDS);
}

static bool temp_should_skip(const char* name)
{
    return name_matches_csv(name, CONFIG_HA_ENTITY_SKIP_TEMP_KEYWORDS);
}

static bool occupancy_should_skip(const char* name)
{
    return name_matches_csv(name, CONFIG_HA_ENTITY_SKIP_OCC_KEYWORDS);
}

// -------------------------------------------------------------------------
// Free widget arrays
// -------------------------------------------------------------------------

static void free_widgets(void)
{
    for (int d = 0; d < DISC_DOMAIN_COUNT; d++)
    {
        if (s_widgets[d])
        {
            heap_caps_free(s_widgets[d]);
            s_widgets[d] = NULL;
        }
        s_widget_counts[d] = 0;
    }
}

// -------------------------------------------------------------------------
// Tab header helpers
// -------------------------------------------------------------------------

static void update_tab_highlight(size_t active_tab)
{
    for (size_t i = 0; i < s_tab_count; i++)
    {
        if (!s_tab_buttons[i])
            continue;
        lv_obj_t* lbl = lv_obj_get_child(s_tab_buttons[i], 0);
        if (!lbl)
            continue;
        lv_color_t color = (i == active_tab) ? TAB_ACTIVE_COLOR : TAB_INACTIVE_COLOR;
        lv_obj_set_style_text_color(lbl, color, 0);

        // Underline active tab
        if (i == active_tab)
            lv_obj_set_style_border_width(s_tab_buttons[i], 2, 0);
        else
            lv_obj_set_style_border_width(s_tab_buttons[i], 0, 0);
    }
}

static void tileview_changed_cb(lv_event_t* e)
{
    lv_obj_t* tv = lv_event_get_current_target(e);
    lv_obj_t* active_tile = lv_tileview_get_tile_act(tv);

    // Find which tab index corresponds to this tile
    uint32_t child_count = lv_obj_get_child_cnt(tv);
    for (uint32_t i = 0; i < child_count && i < s_tab_count; i++)
    {
        if (lv_obj_get_child(tv, (int32_t)i) == active_tile)
        {
            update_tab_highlight(i);
            break;
        }
    }
}

static void tab_btn_clicked_cb(lv_event_t* e)
{
    lv_obj_t* btn = lv_event_get_current_target(e);
    // Find the tab index for this button
    for (size_t i = 0; i < s_tab_count; i++)
    {
        if (s_tab_buttons[i] == btn)
        {
            // Switch tileview to this tab
            lv_obj_t* tile = lv_obj_get_child(s_tileview, (int32_t)i);
            if (tile)
                lv_obj_set_tile(s_tileview, tile, LV_ANIM_ON);
            update_tab_highlight(i);
            break;
        }
    }
}

static lv_obj_t* add_tab_button(lv_obj_t* header, const char* label_text, size_t tab_idx)
{
    lv_obj_t* btn = lv_btn_create(header);
    lv_obj_remove_style_all(btn);
    lv_obj_set_height(btn, TAB_HEADER_H);
    lv_obj_set_width(btn, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(btn, 16, 0);
    lv_obj_set_style_pad_ver(btn, 4, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(btn, TAB_ACTIVE_COLOR, 0);
    lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_add_event_cb(btn, tab_btn_clicked_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label_text);
    lv_obj_add_style(lbl, ui_style_value_secondary(), 0);
    lv_obj_set_style_text_color(lbl, (tab_idx == 0) ? TAB_ACTIVE_COLOR : TAB_INACTIVE_COLOR, 0);
    lv_obj_center(lbl);

    s_tab_buttons[tab_idx] = btn;
    return btn;
}

// -------------------------------------------------------------------------
// Page builders
// -------------------------------------------------------------------------

// Create a scrollable column container inside a tile
static lv_obj_t* make_tile_scroll_container(lv_obj_t* tile)
{
    lv_obj_t* cont = lv_obj_create(tile);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cont, 8, 0);
    lv_obj_set_style_pad_row(cont, 6, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(cont, LV_OPA_TRANSP, 0);
    return cont;
}

// Create a scrollable 2-column grid container inside a tile
static lv_obj_t* make_tile_grid_container(lv_obj_t* tile)
{
    lv_obj_t* cont = lv_obj_create(tile);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(cont, 8, 0);
    lv_obj_set_style_pad_gap(cont, 6, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(cont, LV_OPA_TRANSP, 0);
    return cont;
}

// Create a dark card suitable for sensor/occupancy/climate tiles
static lv_obj_t* make_card(lv_obj_t* parent, bool half_width)
{
    lv_obj_t* card = lv_obj_create(parent);
    if (half_width)
        lv_obj_set_width(card, (SCREEN_W - 28) / 2); // two columns with gaps
    else
        lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_style_pad_row(card, 4, 0);
    lv_obj_set_style_bg_color(card, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_border_opa(card, LV_OPA_TRANSP, 0);
    return card;
}

// --- Lights ---

static void build_lights_page(lv_obj_t* tile, const disc_result_t* disc, size_t domain_idx)
{
    size_t count = disc->domain_counts[DISC_DOMAIN_LIGHT];
    s_widgets[DISC_DOMAIN_LIGHT] =
        heap_caps_calloc(count, sizeof(tab_widget_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_widgets[DISC_DOMAIN_LIGHT])
        s_widgets[DISC_DOMAIN_LIGHT] = calloc(count, sizeof(tab_widget_t));
    if (!s_widgets[DISC_DOMAIN_LIGHT])
    {
        ESP_LOGE(TAG, "OOM lights");
        return;
    }
    s_widget_counts[DISC_DOMAIN_LIGHT] = count;

    lv_obj_t* cont = make_tile_grid_container(tile);
    size_t slot = 0;

    for (size_t i = 0; i < disc->count && slot < count; i++)
    {
        const disc_entity_t* e = &disc->entities[i];
        if (e->domain != DISC_DOMAIN_LIGHT)
            continue;
        if (light_should_skip(e->friendly_name))
        {
            slot++;
            continue;
        }

        tab_widget_t* w = &s_widgets[DISC_DOMAIN_LIGHT][slot];
        snprintf(w->entity_id, sizeof(w->entity_id), "%s", e->entity_id);
        snprintf(w->name, sizeof(w->name), "%s", e->friendly_name);

        lv_obj_t* card = make_card(cont, true);

        // Top row: icon + toggle
        lv_obj_t* top = lv_obj_create(card);
        lv_obj_remove_style_all(top);
        lv_obj_set_width(top, LV_PCT(100));
        lv_obj_set_height(top, LV_SIZE_CONTENT);
        lv_obj_set_layout(top, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);

        lv_obj_t* icon = lv_label_create(top);
        lv_obj_add_style(icon, ui_icon_style(), 0);
        lv_label_set_text(icon, get_room_icon(w->name));
        lv_obj_set_style_text_color(icon, lv_color_hex(0x666666), 0); // dim until state known
        w->label = icon;

        lv_obj_t* toggle = lv_switch_create(top);
        lv_obj_set_user_data(toggle, w);
        lv_obj_add_event_cb(toggle, light_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
        w->toggle = toggle;

        // Name label
        lv_obj_t* name_lbl = lv_label_create(card);
        lv_obj_add_style(name_lbl, ui_style_value_secondary(), 0);
        lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(name_lbl, LV_PCT(100));
        lv_label_set_text(name_lbl, w->name);

        // Brightness slider
        lv_obj_t* slider = lv_slider_create(card);
        lv_obj_set_width(slider, LV_PCT(100));
        lv_obj_set_height(slider, 14);
        lv_slider_set_range(slider, 0, 255);
        lv_obj_set_user_data(slider, w);
        lv_obj_add_event_cb(slider, light_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
        w->slider = slider;

        slot++;
    }
}

// --- Temperature sensors ---

static void build_temperature_page(lv_obj_t* tile, const disc_result_t* disc, size_t domain_idx)
{
    size_t count = disc->domain_counts[DISC_DOMAIN_TEMPERATURE];
    s_widgets[DISC_DOMAIN_TEMPERATURE] =
        heap_caps_calloc(count, sizeof(tab_widget_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_widgets[DISC_DOMAIN_TEMPERATURE])
        s_widgets[DISC_DOMAIN_TEMPERATURE] = calloc(count, sizeof(tab_widget_t));
    if (!s_widgets[DISC_DOMAIN_TEMPERATURE])
    {
        ESP_LOGE(TAG, "OOM temps");
        return;
    }
    s_widget_counts[DISC_DOMAIN_TEMPERATURE] = count;

    lv_obj_t* cont = make_tile_grid_container(tile);
    size_t slot = 0;

    for (size_t i = 0; i < disc->count && slot < count; i++)
    {
        const disc_entity_t* e = &disc->entities[i];
        if (e->domain != DISC_DOMAIN_TEMPERATURE)
            continue;
        if (temp_should_skip(e->friendly_name))
        {
            slot++;
            continue;
        }

        tab_widget_t* w = &s_widgets[DISC_DOMAIN_TEMPERATURE][slot];
        snprintf(w->entity_id, sizeof(w->entity_id), "%s", e->entity_id);
        snprintf(w->name, sizeof(w->name), "%s", e->friendly_name);

        lv_obj_t* card = make_card(cont, true);

        // Thermometer icon
        lv_obj_t* icon = lv_label_create(card);
        lv_obj_add_style(icon, ui_icon_style(), 0);
        lv_label_set_text(icon, ICON_THERMOMETER);
        lv_obj_set_style_text_color(icon, UI_COLOR_ACCENT_TEAL, 0);

        // Temperature value — large and prominent
        lv_obj_t* val_lbl = lv_label_create(card);
        lv_obj_add_style(val_lbl, ui_style_value_primary(), 0);
        lv_label_set_text(val_lbl, "--.-");
        w->label = val_lbl;

        // Room name — muted below value
        lv_obj_t* name_lbl = lv_label_create(card);
        lv_obj_add_style(name_lbl, ui_style_value_secondary(), 0);
        lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(name_lbl, LV_PCT(100));
        lv_label_set_text(name_lbl, w->name);

        slot++;
    }
}

// --- Climate (thermostats) ---

static void build_climate_page(lv_obj_t* tile, const disc_result_t* disc, size_t domain_idx)
{
    size_t count = disc->domain_counts[DISC_DOMAIN_CLIMATE];
    s_widgets[DISC_DOMAIN_CLIMATE] =
        heap_caps_calloc(count, sizeof(tab_widget_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_widgets[DISC_DOMAIN_CLIMATE])
        s_widgets[DISC_DOMAIN_CLIMATE] = calloc(count, sizeof(tab_widget_t));
    if (!s_widgets[DISC_DOMAIN_CLIMATE])
    {
        ESP_LOGE(TAG, "OOM climate");
        return;
    }
    s_widget_counts[DISC_DOMAIN_CLIMATE] = count;

    lv_obj_t* cont = make_tile_scroll_container(tile);

    size_t slot = 0;
    for (size_t i = 0; i < disc->count && slot < count; i++)
    {
        const disc_entity_t* e = &disc->entities[i];
        if (e->domain != DISC_DOMAIN_CLIMATE)
            continue;

        tab_widget_t* w = &s_widgets[DISC_DOMAIN_CLIMATE][slot];
        snprintf(w->entity_id, sizeof(w->entity_id), "%s", e->entity_id);
        snprintf(w->name, sizeof(w->name), "%s", e->friendly_name);
        w->target_temp = 70.0f; // default until first poll

        lv_obj_t* card = make_card(cont, false); // full-width for climate
        lv_obj_set_style_pad_all(card, 12, 0);

        // Top row: icon + name
        lv_obj_t* top = lv_obj_create(card);
        lv_obj_remove_style_all(top);
        lv_obj_set_width(top, LV_PCT(100));
        lv_obj_set_height(top, LV_SIZE_CONTENT);
        lv_obj_set_layout(top, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_gap(top, 8, 0);

        lv_obj_t* icon = lv_label_create(top);
        lv_obj_add_style(icon, ui_icon_style(), 0);
        lv_label_set_text(icon, ICON_THERMOMETER);
        lv_obj_set_style_text_color(icon, UI_COLOR_ACCENT_AMBER, 0);

        lv_obj_t* name_lbl = lv_label_create(top);
        lv_obj_add_style(name_lbl, ui_style_value_secondary(), 0);
        lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_flex_grow(name_lbl, 1);
        lv_label_set_text(name_lbl, w->name);

        // Current temp display: "Now: --.-  Set: --.-"
        lv_obj_t* val_lbl = lv_label_create(card);
        lv_obj_add_style(val_lbl, ui_style_value_primary(), 0);
        lv_label_set_text(val_lbl, "Now: ---  Set: ---");
        w->label = val_lbl;

        // Bottom row: [ - ]  [ setpoint ]  [ + ]
        lv_obj_t* ctrl_row = lv_obj_create(card);
        lv_obj_remove_style_all(ctrl_row);
        lv_obj_set_width(ctrl_row, LV_PCT(100));
        lv_obj_set_height(ctrl_row, LV_SIZE_CONTENT);
        lv_obj_set_layout(ctrl_row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(ctrl_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(ctrl_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(ctrl_row, LV_OPA_TRANSP, 0);

        // Minus button
        lv_obj_t* minus_btn = lv_btn_create(ctrl_row);
        lv_obj_remove_style_all(minus_btn);
        lv_obj_set_size(minus_btn, 54, 44);
        lv_obj_set_style_bg_color(minus_btn, lv_color_hex(0x2A2A2A), 0);
        lv_obj_set_style_bg_opa(minus_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(minus_btn, 8, 0);
        lv_obj_set_user_data(minus_btn, w);
        lv_obj_add_event_cb(minus_btn, climate_temp_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-1);
        lv_obj_t* minus_lbl = lv_label_create(minus_btn);
        lv_label_set_text(minus_lbl, "-");
        lv_obj_add_style(minus_lbl, ui_style_value_primary(), 0);
        lv_obj_center(minus_lbl);

        // Setpoint label (center)
        lv_obj_t* set_lbl = lv_label_create(ctrl_row);
        lv_obj_add_style(set_lbl, ui_style_value_primary(), 0);
        lv_label_set_text(set_lbl, "70 F");
        lv_obj_set_flex_grow(set_lbl, 1);
        lv_obj_set_style_text_align(set_lbl, LV_TEXT_ALIGN_CENTER, 0);
        w->toggle = set_lbl; // repurpose toggle ptr to hold setpoint label

        // Plus button
        lv_obj_t* plus_btn = lv_btn_create(ctrl_row);
        lv_obj_remove_style_all(plus_btn);
        lv_obj_set_size(plus_btn, 54, 44);
        lv_obj_set_style_bg_color(plus_btn, lv_color_hex(0x2A2A2A), 0);
        lv_obj_set_style_bg_opa(plus_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(plus_btn, 8, 0);
        lv_obj_set_user_data(plus_btn, w);
        lv_obj_add_event_cb(plus_btn, climate_temp_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t) + 1);
        lv_obj_t* plus_lbl = lv_label_create(plus_btn);
        lv_label_set_text(plus_lbl, "+");
        lv_obj_add_style(plus_lbl, ui_style_value_primary(), 0);
        lv_obj_center(plus_lbl);

        slot++;
    }
}

// --- Occupancy ---

static void build_occupancy_page(lv_obj_t* tile, const disc_result_t* disc, size_t domain_idx)
{
    size_t count = disc->domain_counts[DISC_DOMAIN_OCCUPANCY];
    s_widgets[DISC_DOMAIN_OCCUPANCY] =
        heap_caps_calloc(count, sizeof(tab_widget_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_widgets[DISC_DOMAIN_OCCUPANCY])
        s_widgets[DISC_DOMAIN_OCCUPANCY] = calloc(count, sizeof(tab_widget_t));
    if (!s_widgets[DISC_DOMAIN_OCCUPANCY])
    {
        ESP_LOGE(TAG, "OOM occ");
        return;
    }
    s_widget_counts[DISC_DOMAIN_OCCUPANCY] = count;

    lv_obj_t* cont = make_tile_grid_container(tile);
    size_t slot = 0;

    for (size_t i = 0; i < disc->count && slot < count; i++)
    {
        const disc_entity_t* e = &disc->entities[i];
        if (e->domain != DISC_DOMAIN_OCCUPANCY)
            continue;
        if (occupancy_should_skip(e->friendly_name))
        {
            slot++;
            continue;
        }

        tab_widget_t* w = &s_widgets[DISC_DOMAIN_OCCUPANCY][slot];
        snprintf(w->entity_id, sizeof(w->entity_id), "%s", e->entity_id);
        snprintf(w->name, sizeof(w->name), "%s", e->friendly_name);

        lv_obj_t* card = make_card(cont, true);

        // Person icon — color reflects occupied state
        lv_obj_t* icon = lv_label_create(card);
        lv_obj_add_style(icon, ui_icon_style(), 0);
        lv_label_set_text(icon, ICON_PERSON);
        lv_obj_set_style_text_color(icon, lv_color_hex(0x444444), 0);
        w->label = icon; // repurpose label ptr to the icon for color updates

        // State text
        lv_obj_t* state_lbl = lv_label_create(card);
        lv_obj_add_style(state_lbl, ui_style_value_primary(), 0);
        lv_label_set_text(state_lbl, "...");

        // Room name
        lv_obj_t* name_lbl = lv_label_create(card);
        lv_obj_add_style(name_lbl, ui_style_value_secondary(), 0);
        lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(name_lbl, LV_PCT(100));
        lv_label_set_text(name_lbl, w->name);

        // Store the state label in toggle ptr (we'll use it in update)
        w->toggle = state_lbl;

        slot++;
    }
}

// --- Media ---

static void build_media_page(lv_obj_t* tile, const disc_result_t* disc, size_t domain_idx)
{
    size_t count = disc->domain_counts[DISC_DOMAIN_MEDIA];
    s_widgets[DISC_DOMAIN_MEDIA] =
        heap_caps_calloc(count, sizeof(tab_widget_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_widgets[DISC_DOMAIN_MEDIA])
        s_widgets[DISC_DOMAIN_MEDIA] = calloc(count, sizeof(tab_widget_t));
    if (!s_widgets[DISC_DOMAIN_MEDIA])
    {
        ESP_LOGE(TAG, "OOM media");
        return;
    }
    s_widget_counts[DISC_DOMAIN_MEDIA] = count;

    lv_obj_t* cont = make_tile_scroll_container(tile);
    size_t slot = 0;

    for (size_t i = 0; i < disc->count && slot < count; i++)
    {
        const disc_entity_t* e = &disc->entities[i];
        if (e->domain != DISC_DOMAIN_MEDIA)
            continue;

        tab_widget_t* w = &s_widgets[DISC_DOMAIN_MEDIA][slot];
        snprintf(w->entity_id, sizeof(w->entity_id), "%s", e->entity_id);
        snprintf(w->name, sizeof(w->name), "%s", e->friendly_name);

        lv_obj_t* card = make_card(cont, false);
        lv_obj_set_style_pad_all(card, 10, 0);

        // Top row: speaker name + play/pause button
        lv_obj_t* top = lv_obj_create(card);
        lv_obj_remove_style_all(top);
        lv_obj_set_width(top, LV_PCT(100));
        lv_obj_set_height(top, LV_SIZE_CONTENT);
        lv_obj_set_layout(top, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);

        lv_obj_t* name_lbl = lv_label_create(top);
        lv_obj_add_style(name_lbl, ui_style_value_primary(), 0);
        lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_flex_grow(name_lbl, 1);
        lv_label_set_text(name_lbl, w->name);
        w->label = name_lbl;

        // Play/pause toggle button
        lv_obj_t* play_btn = lv_btn_create(top);
        lv_obj_remove_style_all(play_btn);
        lv_obj_set_size(play_btn, 54, 38);
        lv_obj_set_style_bg_color(play_btn, lv_color_hex(0x2A2A2A), 0);
        lv_obj_set_style_bg_opa(play_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(play_btn, 8, 0);
        lv_obj_set_user_data(play_btn, w);
        lv_obj_add_event_cb(play_btn, media_play_pause_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t* play_icon = lv_label_create(play_btn);
        lv_obj_set_style_text_color(play_icon, UI_COLOR_SECONDARY_TEXT, 0);
        lv_label_set_text(play_icon, ICON_PLAY);
        lv_obj_center(play_icon);
        w->play_btn = play_icon; // store icon label so we can swap play<->pause

        // Now-playing title (muted, truncated)
        lv_obj_t* title_lbl = lv_label_create(card);
        lv_obj_add_style(title_lbl, ui_style_value_secondary(), 0);
        lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(title_lbl, LV_PCT(100));
        lv_label_set_text(title_lbl, "");
        w->toggle = title_lbl; // repurpose toggle ptr for media title

        // Volume row: speaker icon + slider
        lv_obj_t* vol_row = lv_obj_create(card);
        lv_obj_remove_style_all(vol_row);
        lv_obj_set_width(vol_row, LV_PCT(100));
        lv_obj_set_height(vol_row, LV_SIZE_CONTENT);
        lv_obj_set_layout(vol_row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(vol_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(vol_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_gap(vol_row, 8, 0);
        lv_obj_set_style_bg_opa(vol_row, LV_OPA_TRANSP, 0);

        lv_obj_t* vol_icon = lv_label_create(vol_row);
        lv_obj_set_style_text_color(vol_icon, UI_COLOR_SECONDARY_TEXT, 0);
        lv_label_set_text(vol_icon, ICON_VOLUME);

        lv_obj_t* vol_slider = lv_slider_create(vol_row);
        lv_obj_set_flex_grow(vol_slider, 1);
        lv_obj_set_height(vol_slider, 16);
        lv_slider_set_range(vol_slider, 0, 100);
        lv_obj_set_user_data(vol_slider, w);
        lv_obj_add_event_cb(vol_slider, media_volume_cb, LV_EVENT_VALUE_CHANGED, NULL);
        w->slider = vol_slider;

        slot++;
    }
}

// --- Scenes & Automations ---

static void build_scenes_page(lv_obj_t* tile, const disc_result_t* disc)
{
    // Scenes and automations are fetched live from HA via scene_service
    // (not from discovery), mirroring the original build_scene_labels() pattern.
    lv_obj_t* cont = make_tile_scroll_container(tile);

    scene_t scenes[32];
    int scene_count = scene_service_list_scenes(scenes, 32);
    automation_t autos[32];
    int auto_count = scene_service_list_automations(autos, 32);

    if (scene_count <= 0 && auto_count <= 0)
    {
        lv_obj_t* lbl = lv_label_create(cont);
        lv_label_set_text(lbl, "No scenes or automations available.");
        lv_obj_add_style(lbl, ui_style_value_secondary(), 0);
        return;
    }

    if (scene_count > 0)
    {
        lv_obj_t* hdr = lv_label_create(cont);
        lv_label_set_text(hdr, "Scenes");
        lv_obj_add_style(hdr, ui_style_value_secondary(), 0);

        for (int i = 0; i < scene_count; i++)
        {
            const char* name = scenes[i].name[0] ? scenes[i].name : scenes[i].entity_id;
            lv_obj_t* btn = lv_btn_create(cont);
            lv_obj_remove_style_all(btn);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A2A2A), 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(btn, 8, 0);
            lv_obj_set_width(btn, LV_PCT(100));
            lv_obj_set_height(btn, LV_SIZE_CONTENT);
            lv_obj_set_style_pad_all(btn, 8, 0);

            char* eid = (char*)lv_mem_alloc(strlen(scenes[i].entity_id) + 1);
            if (eid)
            {
                strcpy(eid, scenes[i].entity_id);
                lv_obj_set_user_data(btn, eid);
                lv_obj_add_event_cb(btn, btn_delete_cb, LV_EVENT_DELETE, eid);
            }
            lv_obj_add_event_cb(btn, scene_btn_cb, LV_EVENT_CLICKED, NULL);

            lv_obj_t* lbl = lv_label_create(btn);
            lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
            lv_label_set_text(lbl, name);
            lv_obj_set_style_text_color(lbl, UI_COLOR_ACCENT_TEAL, 0);
            lv_obj_center(lbl);
        }
    }

    if (auto_count > 0)
    {
        lv_obj_t* hdr = lv_label_create(cont);
        lv_label_set_text(hdr, "Automations");
        lv_obj_add_style(hdr, ui_style_value_secondary(), 0);

        for (int i = 0; i < auto_count; i++)
        {
            const char* name = autos[i].name[0] ? autos[i].name : autos[i].entity_id;
            lv_obj_t* btn = lv_btn_create(cont);
            lv_obj_remove_style_all(btn);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A2A2A), 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(btn, 8, 0);
            lv_obj_set_width(btn, LV_PCT(100));
            lv_obj_set_height(btn, LV_SIZE_CONTENT);
            lv_obj_set_style_pad_all(btn, 8, 0);

            if (!autos[i].enabled)
                lv_obj_add_state(btn, LV_STATE_DISABLED);

            char* eid = (char*)lv_mem_alloc(strlen(autos[i].entity_id) + 1);
            if (eid)
            {
                strcpy(eid, autos[i].entity_id);
                lv_obj_set_user_data(btn, eid);
                lv_obj_add_event_cb(btn, btn_delete_cb, LV_EVENT_DELETE, eid);
            }
            lv_obj_add_event_cb(btn, automation_btn_cb, LV_EVENT_CLICKED, NULL);

            lv_obj_t* lbl = lv_label_create(btn);
            lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
            lv_label_set_text(lbl, name);
            lv_obj_set_style_text_color(lbl, UI_COLOR_ACCENT_TEAL, 0);
            lv_obj_center(lbl);
        }
    }
}

// Dispatch to the correct builder for a domain
static void build_page(lv_obj_t* tile, const disc_result_t* disc, disc_domain_t domain,
                       size_t domain_idx)
{
    switch (domain)
    {
    case DISC_DOMAIN_LIGHT:
        build_lights_page(tile, disc, domain_idx);
        break;
    case DISC_DOMAIN_TEMPERATURE:
        build_temperature_page(tile, disc, domain_idx);
        break;
    case DISC_DOMAIN_CLIMATE:
        build_climate_page(tile, disc, domain_idx);
        break;
    case DISC_DOMAIN_OCCUPANCY:
        build_occupancy_page(tile, disc, domain_idx);
        break;
    case DISC_DOMAIN_MEDIA:
        build_media_page(tile, disc, domain_idx);
        break;
    case DISC_DOMAIN_SCENE:
    case DISC_DOMAIN_AUTOMATION:
        build_scenes_page(tile, disc);
        break;
    default:
        break;
    }
}

// -------------------------------------------------------------------------
// Climate inline +/- button callback
// -------------------------------------------------------------------------

static void climate_temp_btn_cb(lv_event_t* e)
{
    lv_obj_t* btn = lv_event_get_target(e);
    tab_widget_t* w = (tab_widget_t*)lv_obj_get_user_data(btn);
    if (!w)
        return;

    intptr_t delta = (intptr_t)lv_event_get_user_data(e);
    w->target_temp += (float)delta; // +1 or -1 degree F

    // Clamp to reasonable thermostat range
    if (w->target_temp < 60.0f)
        w->target_temp = 60.0f;
    if (w->target_temp > 85.0f)
        w->target_temp = 85.0f;

    // Update setpoint label immediately
    if (w->toggle)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f F", w->target_temp);
        lv_label_set_text(w->toggle, buf);
    }
    // Update combined current/target label
    if (w->label)
    {
        char buf[40];
        if (w->temp > 0.0f)
            snprintf(buf, sizeof(buf), "Now: %.1f  Set: %.0f F", w->temp, w->target_temp);
        else
            snprintf(buf, sizeof(buf), "Set: %.0f F", w->target_temp);
        lv_label_set_text(w->label, buf);
    }

    cJSON* params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "entity_id", w->entity_id);
    cJSON_AddNumberToObject(params, "temperature", w->target_temp);
    command_queue_enqueue_service("climate", "set_temperature", params);
    cJSON_Delete(params);
}

// -------------------------------------------------------------------------
// Event callbacks
// -------------------------------------------------------------------------

static void light_switch_cb(lv_event_t* e)
{
    lv_obj_t* sw = lv_event_get_target(e);
    tab_widget_t* w = (tab_widget_t*)lv_obj_get_user_data(sw);
    if (!w)
        return;

    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (on)
        command_queue_enqueue_light_on(w->entity_id);
    else
        command_queue_enqueue_light_off(w->entity_id);
}

static void light_slider_cb(lv_event_t* e)
{
    lv_obj_t* slider = lv_event_get_target(e);
    tab_widget_t* w = (tab_widget_t*)lv_obj_get_user_data(slider);
    if (!w)
        return;

    int32_t value = lv_slider_get_value(slider);
    if (value > 0 && !w->is_on)
    {
        cJSON* params = cJSON_CreateObject();
        cJSON_AddNumberToObject(params, "brightness", value);
        command_queue_enqueue_light(w->entity_id, params);
        cJSON_Delete(params);
    }
    else
    {
        command_queue_enqueue_light_brightness(w->entity_id, (uint8_t)value);
    }
}

static void media_play_pause_cb(lv_event_t* e)
{
    lv_obj_t* btn = lv_event_get_target(e);
    tab_widget_t* w = (tab_widget_t*)lv_obj_get_user_data(btn);
    if (!w)
        return;

    // Toggle based on current icon text (play icon = paused, pause icon = playing)
    bool currently_playing = w->play_btn && strcmp(lv_label_get_text(w->play_btn), ICON_PAUSE) == 0;

    cJSON* payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "entity_id", w->entity_id);
    command_queue_enqueue_service("media_player", currently_playing ? "media_pause" : "media_play",
                                  payload);
    cJSON_Delete(payload);
}

static void media_volume_cb(lv_event_t* e)
{
    lv_obj_t* slider = lv_event_get_target(e);
    tab_widget_t* w = (tab_widget_t*)lv_obj_get_user_data(slider);
    if (!w)
        return;

    int32_t value = lv_slider_get_value(slider);
    float volume_level = (float)value / 100.0f;

    cJSON* payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "entity_id", w->entity_id);
    cJSON_AddNumberToObject(payload, "volume_level", volume_level);
    command_queue_enqueue_service("media_player", "volume_set", payload);
    cJSON_Delete(payload);
}

static void scene_btn_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
        return;
    const char* eid = (const char*)lv_obj_get_user_data(lv_event_get_target(e));
    if (eid)
        scene_service_trigger_scene(eid);
}

static void automation_btn_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
        return;
    const char* eid = (const char*)lv_obj_get_user_data(lv_event_get_target(e));
    if (eid)
        scene_service_trigger_automation(eid);
}

static void btn_delete_cb(lv_event_t* e)
{
    char* eid = (char*)lv_event_get_user_data(e);
    if (eid)
        lv_mem_free(eid);
}

// -------------------------------------------------------------------------
// tabbed_ui_create / tabbed_ui_rebuild
// -------------------------------------------------------------------------

static void build_ui_internal(const disc_result_t* disc)
{
    // Must be called with LVGL mutex held
    lv_obj_t* scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_remove_style_all(scr);
    lv_obj_add_style(scr, ui_style_screen(), 0);

    // Reset tab state
    memset(s_tab_buttons, 0, sizeof(s_tab_buttons));
    memset(s_tab_domain, 0, sizeof(s_tab_domain));
    s_tab_count = 0;
    s_tileview = NULL;
    s_tab_header = NULL;

    free_widgets();

    // --- Status bar ---
    g_status.status_bar = lv_obj_create(scr);
    lv_obj_remove_style_all(g_status.status_bar);
    lv_obj_add_style(g_status.status_bar, ui_style_status_bar(), 0);
    lv_obj_set_size(g_status.status_bar, SCREEN_W, STATUS_BAR_H);
    lv_obj_align(g_status.status_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_pad_column(g_status.status_bar, 12, 0);

    g_status.wifi_indicator = lv_obj_create(g_status.status_bar);
    lv_obj_remove_style_all(g_status.wifi_indicator);
    lv_obj_add_style(g_status.wifi_indicator, ui_style_status_indicator(), 0);
    lv_obj_align(g_status.wifi_indicator, LV_ALIGN_LEFT_MID, 8, 0);

    g_status.wifi_label = lv_label_create(g_status.status_bar);
    lv_obj_add_style(g_status.wifi_label, ui_style_value_secondary(), 0);
    lv_obj_align_to(g_status.wifi_label, g_status.wifi_indicator, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    lv_label_set_text(g_status.wifi_label, "WiFi...");

    g_status.api_indicator = lv_obj_create(g_status.status_bar);
    lv_obj_remove_style_all(g_status.api_indicator);
    lv_obj_add_style(g_status.api_indicator, ui_style_status_indicator(), 0);
    lv_obj_align_to(g_status.api_indicator, g_status.wifi_indicator, LV_ALIGN_OUT_RIGHT_MID, 160,
                    0);

    g_status.api_label = lv_label_create(g_status.status_bar);
    lv_obj_add_style(g_status.api_label, ui_style_value_secondary(), 0);
    lv_obj_align_to(g_status.api_label, g_status.api_indicator, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    lv_label_set_text(g_status.api_label, "API...");

    // --- Tab header ---
    s_tab_header = lv_obj_create(scr);
    lv_obj_remove_style_all(s_tab_header);
    lv_obj_set_size(s_tab_header, SCREEN_W, TAB_HEADER_H);
    lv_obj_set_pos(s_tab_header, 0, STATUS_BAR_H);
    lv_obj_set_style_bg_color(s_tab_header, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(s_tab_header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_tab_header, 0, 0);
    lv_obj_set_scroll_dir(s_tab_header, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(s_tab_header, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_layout(s_tab_header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(s_tab_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_tab_header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(s_tab_header, 4, 0);

    // --- Tileview ---
    s_tileview = lv_tileview_create(scr);
    lv_obj_set_pos(s_tileview, 0, TILEVIEW_Y);
    lv_obj_set_size(s_tileview, SCREEN_W, TILEVIEW_H);
    lv_obj_set_style_bg_opa(s_tileview, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(s_tileview, tileview_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // --- Add one tile per domain that has entities ---
    // DISC_DOMAIN_SCENE and DISC_DOMAIN_AUTOMATION share one page
    bool scenes_added = false;
    size_t tab_idx = 0;

    for (int d = 0; d < DISC_DOMAIN_COUNT && tab_idx < DISC_DOMAIN_COUNT; d++)
    {
        disc_domain_t domain = (disc_domain_t)d;

        // Scene and automation share one tab
        if (domain == DISC_DOMAIN_SCENE || domain == DISC_DOMAIN_AUTOMATION)
        {
            if (scenes_added)
                continue;
            // Check if either has entities
            if (disc->domain_counts[DISC_DOMAIN_SCENE] == 0 &&
                disc->domain_counts[DISC_DOMAIN_AUTOMATION] == 0)
                continue;
            scenes_added = true;

            lv_obj_t* tile = lv_tileview_add_tile(s_tileview, (uint8_t)tab_idx, 0, LV_DIR_HOR);
            build_scenes_page(tile, disc);
            add_tab_button(s_tab_header, "Scenes", tab_idx);
            s_tab_domain[tab_idx] = DISC_DOMAIN_SCENE;
            tab_idx++;
            continue;
        }

        // Switches page is intentionally omitted (smart plugs — risk of accidental toggle)
        if (domain == DISC_DOMAIN_SWITCH)
            continue;

        if (disc->domain_counts[domain] == 0)
            continue;

        lv_obj_t* tile = lv_tileview_add_tile(s_tileview, (uint8_t)tab_idx, 0, LV_DIR_HOR);
        build_page(tile, disc, domain, tab_idx);

        const char* label = discovery_domain_label(domain);
        add_tab_button(s_tab_header, label, tab_idx);
        s_tab_domain[tab_idx] = d;
        tab_idx++;
    }

    s_tab_count = tab_idx;

    if (s_tab_count == 0)
    {
        // No entities — show a "no entities" message on a blank tile
        lv_obj_t* tile = lv_tileview_add_tile(s_tileview, 0, 0, LV_DIR_NONE);
        lv_obj_t* lbl = lv_label_create(tile);
        lv_label_set_text(lbl, "Discovering entities...");
        lv_obj_add_style(lbl, ui_style_value_secondary(), 0);
        lv_obj_center(lbl);
        s_tab_count = 1;
    }

    // Highlight first tab
    if (s_tab_count > 0)
        update_tab_highlight(0);

    s_initialized = true;
}

void tabbed_ui_create(const disc_result_t* disc)
{
    if (!lv_is_initialized())
    {
        ESP_LOGW(TAG, "LVGL not initialized");
        return;
    }

    if (!lvgl_port_lock(-1))
    {
        ESP_LOGE(TAG, "Failed to acquire LVGL mutex");
        return;
    }
    build_ui_internal(disc);
    lvgl_port_unlock();
    ESP_LOGI(TAG, "Tabbed UI created (%zu tabs)", s_tab_count);
}

void tabbed_ui_rebuild(const disc_result_t* disc)
{
    ESP_LOGI(TAG, "Rebuilding tabbed UI");
    s_initialized = false;
    tabbed_ui_create(disc);
}

bool tabbed_ui_ready(void) { return s_initialized; }

// -------------------------------------------------------------------------
// State update functions
// -------------------------------------------------------------------------

void tabbed_ui_update_light(size_t slot, bool on, uint8_t brightness)
{
    if (slot >= s_widget_counts[DISC_DOMAIN_LIGHT] || !s_widgets[DISC_DOMAIN_LIGHT])
        return;
    if (!s_initialized || !lv_is_initialized())
        return;

    tab_widget_t* w = &s_widgets[DISC_DOMAIN_LIGHT][slot];
    w->is_on = on;
    w->brightness = brightness;
    w->has_value = true;

    if (!lvgl_port_lock(-1))
        return;

    // Update icon color
    if (w->label)
    {
        lv_color_t color = on ? UI_COLOR_ACCENT_AMBER : lv_color_hex(0x666666);
        ui_icon_set_color(w->label, color);
    }
    // Update toggle
    if (w->toggle)
    {
        if (on)
            lv_obj_add_state(w->toggle, LV_STATE_CHECKED);
        else
            lv_obj_clear_state(w->toggle, LV_STATE_CHECKED);
    }
    // Update slider
    if (w->slider)
        lv_slider_set_value(w->slider, brightness, LV_ANIM_OFF);

    lvgl_port_unlock();
}

void tabbed_ui_update_temperature(size_t slot, float temp)
{
    if (slot >= s_widget_counts[DISC_DOMAIN_TEMPERATURE] || !s_widgets[DISC_DOMAIN_TEMPERATURE])
        return;
    if (!s_initialized || !lv_is_initialized())
        return;

    tab_widget_t* w = &s_widgets[DISC_DOMAIN_TEMPERATURE][slot];
    w->temp = temp;
    w->has_value = true;

    if (!lvgl_port_lock(-1))
        return;

    if (w->label)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f F", temp);
        lv_label_set_text(w->label, buf);
    }

    lvgl_port_unlock();
}

void tabbed_ui_update_temperature_with_target(size_t slot, float temp, float target_temp)
{
    if (slot >= s_widget_counts[DISC_DOMAIN_CLIMATE] || !s_widgets[DISC_DOMAIN_CLIMATE])
        return;
    if (!s_initialized || !lv_is_initialized())
        return;

    tab_widget_t* w = &s_widgets[DISC_DOMAIN_CLIMATE][slot];
    w->temp = temp;
    // Only update target_temp from HA if user hasn't just pressed +/- (keep local pending value)
    // We update unconditionally here on poll — button cb applies immediately
    w->target_temp = target_temp;
    w->has_value = true;

    if (!lvgl_port_lock(-1))
        return;

    // Combined current/target label
    if (w->label)
    {
        char buf[40];
        if (target_temp > 0.0f)
            snprintf(buf, sizeof(buf), "Now: %.1f  Set: %.0f F", temp, target_temp);
        else
            snprintf(buf, sizeof(buf), "Now: %.1f F", temp);
        lv_label_set_text(w->label, buf);
    }

    // Setpoint label on the control row
    if (w->toggle && target_temp > 0.0f)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f F", target_temp);
        lv_label_set_text(w->toggle, buf);
    }

    lvgl_port_unlock();
}

void tabbed_ui_update_occupancy(size_t slot, bool occupied)
{
    if (slot >= s_widget_counts[DISC_DOMAIN_OCCUPANCY] || !s_widgets[DISC_DOMAIN_OCCUPANCY])
        return;
    if (!s_initialized || !lv_is_initialized())
        return;

    tab_widget_t* w = &s_widgets[DISC_DOMAIN_OCCUPANCY][slot];
    w->occupied = occupied;
    w->has_value = true;

    if (!lvgl_port_lock(-1))
        return;

    // Update icon color (bright lime = occupied, dim = vacant)
    if (w->label)
        lv_obj_set_style_text_color(w->label,
                                    occupied ? UI_COLOR_ACCENT_LIME : lv_color_hex(0x444444), 0);

    // Update state text (stored in toggle ptr for occupancy)
    if (w->toggle)
        lv_label_set_text(w->toggle, occupied ? "Occupied" : "Vacant");

    lvgl_port_unlock();
}

void tabbed_ui_update_media(size_t slot, const char* state, float volume, const char* media_title)
{
    if (slot >= s_widget_counts[DISC_DOMAIN_MEDIA] || !s_widgets[DISC_DOMAIN_MEDIA])
        return;
    if (!s_initialized || !lv_is_initialized())
        return;

    tab_widget_t* w = &s_widgets[DISC_DOMAIN_MEDIA][slot];
    w->has_value = true;

    if (!lvgl_port_lock(-1))
        return;

    // Update now-playing title (toggle ptr)
    if (w->toggle)
    {
        const char* title = (media_title && media_title[0]) ? media_title : "";
        lv_label_set_text(w->toggle, title);
    }

    // Update play/pause icon based on state
    if (w->play_btn)
    {
        bool playing = state && (strcmp(state, "playing") == 0);
        lv_label_set_text(w->play_btn, playing ? ICON_PAUSE : ICON_PLAY);
        lv_color_t color = playing ? UI_COLOR_ACCENT_TEAL : UI_COLOR_SECONDARY_TEXT;
        lv_obj_set_style_text_color(w->play_btn, color, 0);
    }

    // Update volume slider (volume is 0.0–1.0 from HA; negative = unknown, skip)
    if (w->slider && volume >= 0.0f)
        lv_slider_set_value(w->slider, (int32_t)(volume * 100.0f + 0.5f), LV_ANIM_OFF);

    lvgl_port_unlock();
}

// -------------------------------------------------------------------------
// Status bar and offline badge
// -------------------------------------------------------------------------

void update_status_indicators(bool wifi_connected, bool api_ok)
{
    if (!lv_is_initialized() || !lvgl_port_lock(-1))
        return;

    if (g_status.wifi_indicator)
        lv_obj_set_style_bg_color(g_status.wifi_indicator,
                                  wifi_connected ? UI_COLOR_ACCENT_TEAL : UI_COLOR_ACCENT_CORAL, 0);
    if (g_status.wifi_label)
        lv_label_set_text(g_status.wifi_label,
                          wifi_connected ? "WiFi Connected" : "WiFi Disconnected");

    if (g_status.api_indicator)
        lv_obj_set_style_bg_color(g_status.api_indicator,
                                  api_ok ? UI_COLOR_ACCENT_TEAL : UI_COLOR_ACCENT_CORAL, 0);
    if (g_status.api_label)
        lv_label_set_text(g_status.api_label, api_ok ? "API Connected" : "HA Offline");

    lvgl_port_unlock();
}

void tabbed_ui_set_offline(bool offline)
{
    if (!lv_is_initialized() || !lvgl_port_lock(-1))
        return;

    if (offline)
    {
        if (!s_offline_badge)
        {
            s_offline_badge = lv_label_create(lv_scr_act());
            lv_label_set_text(s_offline_badge, "Offline");
            lv_obj_set_style_bg_color(s_offline_badge, lv_color_hex(0xB00020), 0);
            lv_obj_set_style_bg_opa(s_offline_badge, LV_OPA_80, 0);
            lv_obj_set_style_text_color(s_offline_badge, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_pad_all(s_offline_badge, 12, 0);
            lv_obj_align(s_offline_badge, LV_ALIGN_CENTER, 0, 0);
        }
        if (s_tileview)
            lv_obj_add_state(s_tileview, LV_STATE_DISABLED);
    }
    else
    {
        if (s_offline_badge)
        {
            lv_obj_del(s_offline_badge);
            s_offline_badge = NULL;
        }
        if (s_tileview)
            lv_obj_clear_state(s_tileview, LV_STATE_DISABLED);
    }

    lvgl_port_unlock();
}
