#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Screen timeout in seconds (default: 2 minutes)
#define SCREEN_TIMEOUT_SEC 120

// Double-tap detection window in milliseconds
#define DOUBLE_TAP_WINDOW_MS 400

/**
 * @brief Initialize the screen manager
 *
 * Sets up inactivity timer and touch event monitoring for
 * automatic screen timeout and double-tap wake/sleep.
 *
 * @return ESP_OK on success
 */
esp_err_t screen_manager_init(void);

/**
 * @brief Notify screen manager of touch activity
 *
 * Call this from touch event handler to reset inactivity timer
 * and detect double-tap gestures.
 *
 * @param pressed true if touch is pressed, false if released
 */
void screen_manager_touch_activity(bool pressed);

/**
 * @brief Check if screen is currently on
 *
 * @return true if screen is on, false if off
 */
bool screen_manager_is_on(void);

/**
 * @brief Manually turn screen on
 */
void screen_manager_wake(void);

/**
 * @brief Manually turn screen off
 */
void screen_manager_sleep(void);

/**
 * @brief Set screen timeout duration
 *
 * @param timeout_sec Timeout in seconds (0 to disable timeout)
 */
void screen_manager_set_timeout(uint32_t timeout_sec);

#endif // SCREEN_MANAGER_H
