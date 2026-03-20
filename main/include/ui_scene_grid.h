#pragma once

#include "lvgl.h"

lv_obj_t* ui_scene_grid_create(lv_obj_t* parent);
lv_obj_t* ui_scene_grid_create_page(lv_obj_t* parent, int page);
void ui_scene_grid_next_page(lv_obj_t* grid);
void ui_scene_grid_prev_page(lv_obj_t* grid);
