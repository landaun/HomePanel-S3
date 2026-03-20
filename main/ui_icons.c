#include "ui_icons.h"
#include "ui_style.h"

static lv_style_t style_icon;
static lv_style_t style_icon_large;

void ui_icons_init(void)
{
    // Initialize base icon style
    lv_style_init(&style_icon);
    lv_style_set_text_font(&style_icon, &ui_icons_font);
    lv_style_set_text_color(&style_icon, UI_COLOR_PRIMARY_TEXT);

    // Initialize large icon style
    lv_style_init(&style_icon_large);
    lv_style_set_text_font(&style_icon_large, &ui_icons_font);
    lv_style_set_text_color(&style_icon_large, UI_COLOR_PRIMARY_TEXT);
}

// Helper functions for icon colors
void ui_icon_set_color(lv_obj_t* icon, lv_color_t color)
{
    lv_obj_set_style_text_color(icon, color, 0);
}

lv_style_t* ui_icon_style(void) { return &style_icon; }

lv_style_t* ui_icon_style_large(void) { return &style_icon_large; }
