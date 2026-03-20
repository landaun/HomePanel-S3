#include "lvgl_port.h"

#include <windows.h>

static CRITICAL_SECTION s_lvgl_mutex;
static bool s_mutex_ready = false;

static void ensure_mutex(void)
{
    if (!s_mutex_ready)
    {
        InitializeCriticalSection(&s_lvgl_mutex);
        s_mutex_ready = true;
    }
}

esp_err_t lvgl_port_init(esp_lcd_panel_handle_t lcd_handle, esp_lcd_touch_handle_t tp_handle)
{
    (void)lcd_handle;
    (void)tp_handle;
    ensure_mutex();
    return ESP_OK;
}

bool lvgl_port_lock(int timeout_ms)
{
    (void)timeout_ms;
    ensure_mutex();
    EnterCriticalSection(&s_lvgl_mutex);
    return true;
}

void lvgl_port_unlock(void)
{
    if (s_mutex_ready)
    {
        LeaveCriticalSection(&s_lvgl_mutex);
    }
}

bool lvgl_port_notify_rgb_vsync(void)
{
    return false;
}
