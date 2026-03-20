#pragma once

#include "lvgl.h"
#include <stdint.h>

uint16_t kelvin_to_mired(uint16_t kelvin);
uint16_t mired_to_kelvin(uint16_t mired);
lv_obj_t* ui_ct_slider_create(lv_obj_t* parent, uint16_t min_kelvin, uint16_t max_kelvin);
lv_obj_t* ui_colorwheel_create(lv_obj_t* parent);
lv_obj_t* ui_color_panel_show(const char* entity_id);
