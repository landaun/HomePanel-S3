#pragma once

#include <stdbool.h>

void offline_tracker_init(int threshold);
bool offline_tracker_update(bool api_ok);
void offline_tracker_reset(void);
