#include "ui_color_panel.h"
#include "command_queue.h"

static lv_timer_t* ct_settle_timer;
static uint16_t ct_pending_mired;
static const char* ct_entity;

static void ct_settle_cb(lv_timer_t* t)
{
    LV_UNUSED(t);
    command_queue_enqueue_light_color_temp(ct_entity, ct_pending_mired);
    lv_timer_del(ct_settle_timer);
    ct_settle_timer = NULL;
}

uint16_t kelvin_to_mired(uint16_t kelvin)
{
    if (!kelvin)
    {
        return 0;
    }
    return (uint16_t)(1000000U / kelvin);
}

uint16_t mired_to_kelvin(uint16_t mired)
{
    if (!mired)
    {
        return 0;
    }
    return (uint16_t)(1000000U / mired);
}

lv_obj_t* ui_ct_slider_create(lv_obj_t* parent, uint16_t min_kelvin, uint16_t max_kelvin)
{
    lv_obj_t* slider = lv_slider_create(parent);
    lv_slider_set_range(slider, kelvin_to_mired(max_kelvin), kelvin_to_mired(min_kelvin));
    lv_obj_set_width(slider, lv_pct(90));
    return slider;
}

static void ct_slider_cb(lv_event_t* e)
{
    static uint32_t last_ms = 0;
    lv_event_code_t code = lv_event_get_code(e);
    const char* entity_id = lv_event_get_user_data(e);
    lv_obj_t* slider = lv_event_get_target(e);
    uint16_t mired = (uint16_t)lv_slider_get_value(slider);

    uint32_t now = lv_tick_get();
    if (code == LV_EVENT_VALUE_CHANGED && now - last_ms < 100)
    {
        ct_pending_mired = mired;
        ct_entity = entity_id;
        if (ct_settle_timer)
            lv_timer_reset(ct_settle_timer);
        else
            ct_settle_timer = lv_timer_create(ct_settle_cb, 300, NULL);
        return;
    }
    last_ms = now;
    command_queue_enqueue_light_color_temp(entity_id, mired);
    ct_pending_mired = mired;
    ct_entity = entity_id;
    if (ct_settle_timer)
        lv_timer_reset(ct_settle_timer);
    else
        ct_settle_timer = lv_timer_create(ct_settle_cb, 300, NULL);
}

lv_obj_t* ui_colorwheel_create(lv_obj_t* parent)
{
    lv_obj_t* wheel = lv_colorwheel_create(parent, true);
    lv_obj_set_size(wheel, 200, 200);
    return wheel;
}

static lv_timer_t* color_settle_timer;
static lv_color_hsv_t color_pending;
static const char* color_entity;

static void color_settle_cb(lv_timer_t* t)
{
    LV_UNUSED(t);
    command_queue_enqueue_light_hs(color_entity, (float)color_pending.h, (float)color_pending.s);
    lv_timer_del(color_settle_timer);
    color_settle_timer = NULL;
}

static void colorwheel_cb(lv_event_t* e)
{
    static uint32_t last_ms = 0;
    lv_event_code_t code = lv_event_get_code(e);
    const char* entity_id = lv_event_get_user_data(e);
    lv_obj_t* wheel = lv_event_get_target(e);
    lv_color_hsv_t hsv = lv_colorwheel_get_hsv(wheel);

    uint32_t now = lv_tick_get();
    if (code == LV_EVENT_VALUE_CHANGED && now - last_ms < 100)
    {
        color_pending = hsv;
        color_entity = entity_id;
        if (color_settle_timer)
            lv_timer_reset(color_settle_timer);
        else
            color_settle_timer = lv_timer_create(color_settle_cb, 300, NULL);
        return;
    }
    last_ms = now;
    command_queue_enqueue_light_hs(entity_id, (float)hsv.h, (float)hsv.s);
    color_pending = hsv;
    color_entity = entity_id;
    if (color_settle_timer)
        lv_timer_reset(color_settle_timer);
    else
        color_settle_timer = lv_timer_create(color_settle_cb, 300, NULL);
}

static void close_btn_cb(lv_event_t* e)
{
    lv_obj_t* panel = lv_event_get_user_data(e);
    lv_obj_del(panel);
}

lv_obj_t* ui_color_panel_show(const char* entity_id)
{
    lv_obj_t* panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(panel, lv_pct(100), lv_pct(100));
    lv_obj_center(panel);

    lv_obj_t* close = lv_btn_create(panel);
    lv_obj_set_size(close, 40, 40);
    lv_obj_align(close, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_add_event_cb(close, close_btn_cb, LV_EVENT_CLICKED, panel);
    lv_obj_t* close_label = lv_label_create(close);
    lv_label_set_text(close_label, "X");
    lv_obj_center(close_label);

    lv_obj_t* wheel = ui_colorwheel_create(panel);
    lv_obj_align(wheel, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_add_event_cb(wheel, colorwheel_cb, LV_EVENT_VALUE_CHANGED, (void*)entity_id);
    lv_obj_add_event_cb(wheel, colorwheel_cb, LV_EVENT_RELEASED, (void*)entity_id);

    lv_obj_t* slider = ui_ct_slider_create(panel, 2700, 6500);
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(slider, ct_slider_cb, LV_EVENT_VALUE_CHANGED, (void*)entity_id);
    lv_obj_add_event_cb(slider, ct_slider_cb, LV_EVENT_RELEASED, (void*)entity_id);

    return panel;
}
