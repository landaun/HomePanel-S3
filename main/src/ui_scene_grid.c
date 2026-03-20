#include "ui_scene_grid.h"
#include "scene_service.h"

#define UI_SCENE_GRID_PAGE_SIZE 8

static int s_page;
static void populate_page(lv_obj_t* grid);
static void grid_gesture_cb(lv_event_t* e);
static void button_delete_cb(lv_event_t* e);

static void scene_btn_event_cb(lv_event_t* e)
{
    const char* entity = lv_event_get_user_data(e);
    lv_obj_t* btn = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (!entity)
    {
        return;
    }
    if (code == LV_EVENT_CLICKED)
    {
        scene_service_trigger_scene(entity);
    }
    else if (code == LV_EVENT_LONG_PRESSED)
    {
        scene_service_toggle_favorite(entity);
        if (scene_service_is_favorite(entity))
        {
            lv_obj_add_state(btn, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(btn, LV_STATE_CHECKED);
        }
    }
}

static void automation_btn_event_cb(lv_event_t* e)
{
    const char* entity = lv_event_get_user_data(e);
    lv_obj_t* btn = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (!entity)
    {
        return;
    }
    if (code == LV_EVENT_CLICKED)
    {
        scene_service_trigger_automation(entity);
    }
    else if (code == LV_EVENT_LONG_PRESSED)
    {
        scene_service_toggle_favorite(entity);
        if (scene_service_is_favorite(entity))
        {
            lv_obj_add_state(btn, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(btn, LV_STATE_CHECKED);
        }
    }
}

lv_obj_t* ui_scene_grid_create(lv_obj_t* parent)
{
    s_page = 0;
    if (!parent)
    {
        return NULL;
    }
    lv_obj_t* grid = lv_obj_create(parent);
    lv_obj_set_size(grid, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_add_event_cb(grid, grid_gesture_cb, LV_EVENT_GESTURE, NULL);
    populate_page(grid);
    return grid;
}

lv_obj_t* ui_scene_grid_create_page(lv_obj_t* parent, int page)
{
    s_page = page;
    return ui_scene_grid_create(parent);
}

void ui_scene_grid_next_page(lv_obj_t* grid)
{
    s_page++;
    populate_page(grid);
}

void ui_scene_grid_prev_page(lv_obj_t* grid)
{
    if (s_page > 0)
    {
        s_page--;
        populate_page(grid);
    }
}

static void populate_page(lv_obj_t* grid)
{
    lv_obj_clean(grid);

    int start = s_page * UI_SCENE_GRID_PAGE_SIZE;

    scene_t scenes[UI_SCENE_GRID_PAGE_SIZE * (s_page + 1)];
    int scount = scene_service_list_scenes(scenes, sizeof(scenes) / sizeof(scenes[0]));
    for (int i = start; i < scount && i < start + UI_SCENE_GRID_PAGE_SIZE; ++i)
    {
        lv_obj_t* btn = lv_btn_create(grid);
        lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_YELLOW),
                                  LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_t* label = lv_label_create(btn);
        const char* name = scenes[i].name[0] ? scenes[i].name : scenes[i].entity_id;
        lv_label_set_text(label, name);
        char* id = lv_mem_alloc(strlen(scenes[i].entity_id) + 1);
        if (id)
        {
            strcpy(id, scenes[i].entity_id);
            lv_obj_add_event_cb(btn, scene_btn_event_cb, LV_EVENT_ALL, id);
            lv_obj_add_event_cb(btn, button_delete_cb, LV_EVENT_DELETE, id);
        }
        if (scene_service_is_favorite(scenes[i].entity_id))
        {
            lv_obj_add_state(btn, LV_STATE_CHECKED);
        }
    }

    automation_t automations[UI_SCENE_GRID_PAGE_SIZE * (s_page + 1)];
    int acount =
        scene_service_list_automations(automations, sizeof(automations) / sizeof(automations[0]));
    for (int i = start; i < acount && i < start + UI_SCENE_GRID_PAGE_SIZE; ++i)
    {
        lv_obj_t* btn = lv_btn_create(grid);
        lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_YELLOW),
                                  LV_PART_MAIN | LV_STATE_CHECKED);
        if (!automations[i].enabled)
        {
            lv_obj_add_state(btn, LV_STATE_DISABLED);
        }
        lv_obj_t* label = lv_label_create(btn);
        const char* name = automations[i].name[0] ? automations[i].name : automations[i].entity_id;
        lv_label_set_text(label, name);
        char* id = lv_mem_alloc(strlen(automations[i].entity_id) + 1);
        if (id)
        {
            strcpy(id, automations[i].entity_id);
            lv_obj_add_event_cb(btn, automation_btn_event_cb, LV_EVENT_ALL, id);
            lv_obj_add_event_cb(btn, button_delete_cb, LV_EVENT_DELETE, id);
        }
        if (scene_service_is_favorite(automations[i].entity_id))
        {
            lv_obj_add_state(btn, LV_STATE_CHECKED);
        }
    }
}

static void grid_gesture_cb(lv_event_t* e)
{
    lv_obj_t* grid = lv_event_get_target(e);
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT)
    {
        ui_scene_grid_next_page(grid);
    }
    else if (dir == LV_DIR_RIGHT)
    {
        ui_scene_grid_prev_page(grid);
    }
}

static void button_delete_cb(lv_event_t* e)
{
    char* id = (char*)lv_event_get_user_data(e);
    if (id)
    {
        lv_mem_free(id);
    }
}
