#include "win32_port.h"

#include "discovery.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lvgl_port.h"
#include "tabbed_ui.h"

#include "sim_data.h"

#include <string.h>
#include <windows.h>

// ---------------------------------------------------------------------------
// Hardcoded stub discovery result for the simulator.
// Slot indices here must match the constants used in sim_data.c.
// ---------------------------------------------------------------------------

#define SIM_LIGHT_COUNT  2
#define SIM_TEMP_COUNT   2
#define SIM_OCC_COUNT    1
#define SIM_MEDIA_COUNT  1
#define SIM_SCENE_COUNT  2

static disc_entity_t s_sim_entities[] = {
    // Lights (slots 0, 1)
    {"light.living_room",  "Living Room",    "", "", "", DISC_DOMAIN_LIGHT,       true},
    {"light.bedroom",      "Bedroom",        "", "", "", DISC_DOMAIN_LIGHT,       true},
    // Temperature (slots 0, 1)
    {"sensor.living_room_temperature", "Living Room Temp", "", "F", "", DISC_DOMAIN_TEMPERATURE, true},
    {"sensor.bedroom_temperature",     "Bedroom Temp",     "", "F", "", DISC_DOMAIN_TEMPERATURE, true},
    // Occupancy (slot 0)
    {"binary_sensor.living_room_occupancy", "Living Room", "", "", "occupancy", DISC_DOMAIN_OCCUPANCY, true},
    // Media (slot 0)
    {"media_player.living_room", "Living Room Speaker", "", "", "", DISC_DOMAIN_MEDIA, true},
    // Scenes (slots 0, 1)
    {"scene.good_morning", "Good Morning",   "", "", "", DISC_DOMAIN_SCENE,       true},
    {"scene.good_night",   "Good Night",     "", "", "", DISC_DOMAIN_SCENE,       true},
};

static disc_result_t s_sim_disc;

static void build_sim_disc(void)
{
    memset(&s_sim_disc, 0, sizeof(s_sim_disc));
    s_sim_disc.entities = s_sim_entities;
    s_sim_disc.count = sizeof(s_sim_entities) / sizeof(s_sim_entities[0]);
    s_sim_disc.domain_counts[DISC_DOMAIN_LIGHT]       = SIM_LIGHT_COUNT;
    s_sim_disc.domain_counts[DISC_DOMAIN_TEMPERATURE] = SIM_TEMP_COUNT;
    s_sim_disc.domain_counts[DISC_DOMAIN_OCCUPANCY]   = SIM_OCC_COUNT;
    s_sim_disc.domain_counts[DISC_DOMAIN_MEDIA]       = SIM_MEDIA_COUNT;
    s_sim_disc.domain_counts[DISC_DOMAIN_SCENE]       = SIM_SCENE_COUNT;
}

int main(void)
{
    ESP_LOGI("sim_main", "Starting HomePanel-S3 simulator");

    lv_init();

    if (!win32_port_init())
    {
        ESP_LOGE("sim_main", "Failed to initialize Win32 port");
        return 1;
    }

    lvgl_port_init(NULL, NULL);

    build_sim_disc();
    tabbed_ui_create(&s_sim_disc);
    update_status_indicators(true, true);

    sim_data_init(&s_sim_disc);

    ULONGLONG last_tick = GetTickCount64();

    while (win32_port_process_events())
    {
        ULONGLONG now = GetTickCount64();
        uint32_t diff = (uint32_t)(now - last_tick);
        if (diff > 0)
        {
            lv_tick_inc(diff);
            last_tick = now;
        }
        lv_timer_handler();
        Sleep(5);
    }

    win32_port_deinit();
    ESP_LOGI("sim_main", "Simulator stopped");
    return 0;
}
