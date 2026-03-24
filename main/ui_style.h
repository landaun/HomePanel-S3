#pragma once

#include "lvgl.h"

#define UI_COLOR_BG lv_color_hex(0x121212)
#define UI_COLOR_SURFACE lv_color_hex(0x1E1E1E)
#define UI_COLOR_CARD lv_color_hex(0x242424)
#define UI_COLOR_BORDER lv_color_hex(0x2E2E2E)
#define UI_COLOR_PRIMARY_TEXT lv_color_hex(0xE0E0E0)
#define UI_COLOR_SECONDARY_TEXT lv_color_hex(0x9E9E9E)
#define UI_COLOR_ACCENT_TEAL lv_color_hex(0x1DE9B6)
#define UI_COLOR_ACCENT_AMBER lv_color_hex(0xFFB74D)
#define UI_COLOR_ACCENT_LIME lv_color_hex(0x76FF03)
#define UI_COLOR_ACCENT_CORAL lv_color_hex(0xFF7043)

lv_style_t* ui_style_card(void);
lv_style_t* ui_style_status_bar(void);
lv_style_t* ui_style_status_indicator(void);
lv_style_t* ui_style_screen(void);
lv_style_t* ui_style_section_header(void);
lv_style_t* ui_style_value_primary(void);
lv_style_t* ui_style_value_secondary(void);

void ui_style_init(void);
