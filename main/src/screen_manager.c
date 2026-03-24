#include "screen_manager.h"
#include "lcd_rgb_port.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char* TAG = "screen_mgr";

// Screen state
static bool s_screen_on = true;
static uint32_t s_timeout_sec = SCREEN_TIMEOUT_SEC;
static esp_timer_handle_t s_timeout_timer = NULL;
static SemaphoreHandle_t s_mutex = NULL;

// Double-tap detection
static int64_t s_last_tap_time = 0;
static bool s_was_pressed = false;

// Forward declarations
static void timeout_callback(void* arg);
static void reset_timeout_timer(void);

esp_err_t screen_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing screen manager (timeout=%lu sec)", (unsigned long)s_timeout_sec);

    // Create mutex
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex)
    {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    // Create timeout timer
    const esp_timer_create_args_t timer_args = {.callback = timeout_callback,
                                                .arg = NULL,
                                                .dispatch_method = ESP_TIMER_TASK,
                                                .name = "screen_timeout"};

    esp_err_t ret = esp_timer_create(&timer_args, &s_timeout_timer);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create timeout timer: %s", esp_err_to_name(ret));
        return ret;
    }

    // Start the timeout timer
    s_screen_on = true;
    reset_timeout_timer();

    ESP_LOGI(TAG, "Screen manager initialized");
    return ESP_OK;
}

void screen_manager_touch_activity(bool pressed)
{
    if (!s_mutex)
        return;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    int64_t now = esp_timer_get_time() / 1000; // Convert to milliseconds

    // Handle touch release (tap detection)
    if (s_was_pressed && !pressed)
    {
        // This is a tap (release after press)
        int64_t time_since_last_tap = now - s_last_tap_time;

        if (s_screen_on)
        {
            // Screen is on - check for double-tap to sleep
            if (time_since_last_tap < DOUBLE_TAP_WINDOW_MS && s_last_tap_time > 0)
            {
                ESP_LOGI(TAG, "Double-tap detected - turning screen off");
                xSemaphoreGive(s_mutex);
                screen_manager_sleep();
                return;
            }
            // Reset inactivity timer on any touch
            reset_timeout_timer();
        }
        else
        {
            // Screen is off - check for double-tap to wake
            if (time_since_last_tap < DOUBLE_TAP_WINDOW_MS && s_last_tap_time > 0)
            {
                ESP_LOGI(TAG, "Double-tap detected - turning screen on");
                xSemaphoreGive(s_mutex);
                screen_manager_wake();
                return;
            }
        }

        s_last_tap_time = now;
    }

    s_was_pressed = pressed;

    // Reset timeout on any activity when screen is on
    if (s_screen_on && pressed)
    {
        reset_timeout_timer();
    }

    xSemaphoreGive(s_mutex);
}

bool screen_manager_is_on(void) { return s_screen_on; }

void screen_manager_wake(void)
{
    if (!s_mutex)
        return;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (!s_screen_on)
    {
        ESP_LOGI(TAG, "Waking screen");
        lcd_rgb_bl_on();
        s_screen_on = true;
        s_last_tap_time = 0; // Reset tap tracking
        reset_timeout_timer();
    }

    xSemaphoreGive(s_mutex);
}

void screen_manager_sleep(void)
{
    if (!s_mutex)
        return;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (s_screen_on)
    {
        ESP_LOGI(TAG, "Putting screen to sleep");

        // Stop the timeout timer
        if (s_timeout_timer)
        {
            esp_timer_stop(s_timeout_timer);
        }

        lcd_rgb_bl_off();
        s_screen_on = false;
        s_last_tap_time = 0; // Reset tap tracking
    }

    xSemaphoreGive(s_mutex);
}

void screen_manager_set_timeout(uint32_t timeout_sec)
{
    if (!s_mutex)
        return;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_timeout_sec = timeout_sec;
    if (s_screen_on)
    {
        reset_timeout_timer();
    }
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Screen timeout set to %lu seconds", (unsigned long)timeout_sec);
}

static void timeout_callback(void* arg)
{
    ESP_LOGI(TAG, "Screen timeout - turning off");
    screen_manager_sleep();
}

static void reset_timeout_timer(void)
{
    if (!s_timeout_timer || s_timeout_sec == 0)
        return;

    // Stop any existing timer
    esp_timer_stop(s_timeout_timer);

    // Start timer with new timeout
    uint64_t timeout_us = (uint64_t)s_timeout_sec * 1000000;
    esp_err_t ret = esp_timer_start_once(s_timeout_timer, timeout_us);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start timeout timer: %s", esp_err_to_name(ret));
    }
}
