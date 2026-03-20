#include "ui_style.h"

static lv_style_t style_screen;
static lv_style_t style_card;
static lv_style_t style_section_header;
static lv_style_t style_value_primary;
static lv_style_t style_value_secondary;
static lv_style_t style_status_bar;
static lv_style_t style_status_indicator;

void ui_style_init(void)
{
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, UI_COLOR_BG);
    lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);
    lv_style_set_text_color(&style_screen, UI_COLOR_PRIMARY_TEXT);

    lv_style_init(&style_card);
    lv_style_set_radius(&style_card, 12);
    lv_style_set_bg_color(&style_card, UI_COLOR_CARD);
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_border_color(&style_card, UI_COLOR_BORDER);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_border_opa(&style_card, LV_OPA_50);
    lv_style_set_pad_all(&style_card, 10);
    lv_style_set_pad_row(&style_card, 4);
    lv_style_set_pad_column(&style_card, 4);

    lv_style_init(&style_section_header);
    lv_style_set_text_color(&style_section_header, UI_COLOR_ACCENT_TEAL);
    lv_style_set_text_opa(&style_section_header, LV_OPA_COVER);
    lv_style_set_pad_bottom(&style_section_header, 4);
    lv_style_set_bg_opa(&style_section_header, LV_OPA_TRANSP);

    lv_style_init(&style_value_primary);
    lv_style_set_text_color(&style_value_primary, UI_COLOR_PRIMARY_TEXT);
    lv_style_set_text_opa(&style_value_primary, LV_OPA_COVER);

    lv_style_init(&style_value_secondary);
    lv_style_set_text_color(&style_value_secondary, UI_COLOR_SECONDARY_TEXT);
    lv_style_set_text_opa(&style_value_secondary, LV_OPA_COVER);

    lv_style_init(&style_status_bar);
    lv_style_set_bg_color(&style_status_bar, UI_COLOR_SURFACE);
    lv_style_set_bg_opa(&style_status_bar, LV_OPA_COVER);
    lv_style_set_text_color(&style_status_bar, UI_COLOR_PRIMARY_TEXT);
    lv_style_set_pad_all(&style_status_bar, 6);
    lv_style_set_height(&style_status_bar, 28);
    lv_style_set_border_color(&style_status_bar, UI_COLOR_BORDER);
    lv_style_set_border_width(&style_status_bar, 1);
    lv_style_set_border_opa(&style_status_bar, LV_OPA_40);

    lv_style_init(&style_status_indicator);
    lv_style_set_radius(&style_status_indicator, 4);
    lv_style_set_width(&style_status_indicator, 10);
    lv_style_set_height(&style_status_indicator, 10);
    lv_style_set_bg_color(&style_status_indicator, UI_COLOR_ACCENT_TEAL);
    lv_style_set_bg_opa(&style_status_indicator, LV_OPA_COVER);
}

lv_style_t* ui_style_screen(void) { return &style_screen; }

lv_style_t* ui_style_card(void) { return &style_card; }

lv_style_t* ui_style_section_header(void) { return &style_section_header; }

lv_style_t* ui_style_value_primary(void) { return &style_value_primary; }

lv_style_t* ui_style_value_secondary(void) { return &style_value_secondary; }

lv_style_t* ui_style_status_bar(void) { return &style_status_bar; }

lv_style_t* ui_style_status_indicator(void) { return &style_status_indicator; }
