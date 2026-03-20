#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "lvgl.h"

#define LVGL_PORT_H_RES 800
#define LVGL_PORT_V_RES 480

typedef void* esp_lcd_panel_handle_t;
typedef void* esp_lcd_touch_handle_t;

esp_err_t lvgl_port_init(esp_lcd_panel_handle_t lcd_handle, esp_lcd_touch_handle_t tp_handle);
bool lvgl_port_lock(int timeout_ms);
void lvgl_port_unlock(void);
bool lvgl_port_notify_rgb_vsync(void);
