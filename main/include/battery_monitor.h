#pragma once

#include <stdint.h>

void battery_monitor_init(void);
uint8_t battery_monitor_get_percent(void);
