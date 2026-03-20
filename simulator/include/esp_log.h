#pragma once

#include <stdarg.h>

void simulator_log_write(const char* level, const char* tag, const char* fmt, ...);

#define ESP_LOGI(TAG, ...) simulator_log_write("I", TAG, __VA_ARGS__)
#define ESP_LOGW(TAG, ...) simulator_log_write("W", TAG, __VA_ARGS__)
#define ESP_LOGE(TAG, ...) simulator_log_write("E", TAG, __VA_ARGS__)
#define ESP_LOGD(TAG, ...) simulator_log_write("D", TAG, __VA_ARGS__)
