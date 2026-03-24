#ifndef UI_ICONS_H
#define UI_ICONS_H

#include "lvgl.h"

LV_FONT_DECLARE(ui_icons_font);

// Unicode icons from Font Awesome 6 Free
#define ICON_THERMOMETER "\xEF\x8B\x89" // U+F2C9
#define ICON_PERSON "\xEF\x80\x87"      // U+F007
#define ICON_LIGHTBULB "\xEF\x83\xAB"   // U+F0EB
#define ICON_WIFI "\xEF\x87\xAB"        // U+F1EB
#define ICON_SCENE "\xEF\x84\x8E"       // U+F10E (fa-sliders)

// Room-specific icons
#define ICON_FIRE "\xEF\x81\xAD"      // U+F06D (fireplace)
#define ICON_BRIEFCASE "\xEF\x82\xB1" // U+F0B1 (office)
#define ICON_UTENSILS "\xEF\x8B\xA7"  // U+F2E7 (dining room)
#define ICON_PLAY LV_SYMBOL_PLAY
#define ICON_PAUSE LV_SYMBOL_PAUSE
#define ICON_VOLUME LV_SYMBOL_VOLUME_MAX

// Icon styles
lv_style_t* ui_icon_style(void);
lv_style_t* ui_icon_style_large(void);

void ui_icon_set_color(lv_obj_t* icon, lv_color_t color);

// Initialize icon styles
void ui_icons_init(void);

#endif // UI_ICONS_H
