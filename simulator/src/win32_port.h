#pragma once

#include <stdbool.h>
#include <windows.h>

#include "lvgl.h"

bool win32_port_init(void);
void win32_port_deinit(void);
bool win32_port_process_events(void);
