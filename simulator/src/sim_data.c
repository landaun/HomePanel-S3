#include "sim_data.h"

#include "esp_log.h"
#include "lvgl.h"
#include "tabbed_ui.h"

#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

static const char* TAG = "sim_data";

static float s_temp_values[16];
static size_t s_temp_count = 0;
static bool s_occ_states[16];
static size_t s_occ_count = 0;

static void temp_timer_cb(lv_timer_t* timer)
{
    (void)timer;
    for (size_t i = 0; i < s_temp_count; ++i)
    {
        float delta = ((float)(rand() % 7) - 3.0f) * 0.1f;
        float val = s_temp_values[i] + delta;
        if (val < 60.0f) val = 60.0f;
        if (val > 80.0f) val = 80.0f;
        s_temp_values[i] = val;
        tabbed_ui_update_temperature(i, val);
    }
}

static void occupancy_timer_cb(lv_timer_t* timer)
{
    (void)timer;
    if (s_occ_count == 0) return;
    size_t index = (size_t)(rand() % s_occ_count);
    s_occ_states[index] = !s_occ_states[index];
    tabbed_ui_update_occupancy(index, s_occ_states[index]);
}

void sim_data_init(const disc_result_t* disc)
{
    srand((unsigned)time(NULL));

    size_t light_count = disc->domain_counts[DISC_DOMAIN_LIGHT];
    ESP_LOGI(TAG, "Lighting entries: %u", (unsigned)light_count);
    for (size_t i = 0; i < light_count; ++i)
    {
        bool on = (i % 2) == 0;
        tabbed_ui_update_light(i, on, on ? 200 : 0);
    }

    s_temp_count = disc->domain_counts[DISC_DOMAIN_TEMPERATURE];
    ESP_LOGI(TAG, "Temperature entries: %u", (unsigned)s_temp_count);
    for (size_t i = 0; i < s_temp_count; ++i)
    {
        s_temp_values[i] = 71.5f + (float)(i * 1.3f);
        tabbed_ui_update_temperature(i, s_temp_values[i]);
    }

    s_occ_count = disc->domain_counts[DISC_DOMAIN_OCCUPANCY];
    ESP_LOGI(TAG, "Occupancy entries: %u", (unsigned)s_occ_count);
    for (size_t i = 0; i < s_occ_count; ++i)
    {
        s_occ_states[i] = (i % 2) == 1;
        tabbed_ui_update_occupancy(i, s_occ_states[i]);
    }

    lv_timer_create(temp_timer_cb, 2500, NULL);
    lv_timer_create(occupancy_timer_cb, 4000, NULL);

    ESP_LOGI(TAG, "Simulator data initialized");
}
