#include "offline_tracker.h"

static int s_threshold;
static int s_fail_count;
static bool s_offline;

void offline_tracker_init(int threshold)
{
    s_threshold = threshold;
    s_fail_count = 0;
    s_offline = false;
}

bool offline_tracker_update(bool api_ok)
{
    if (!api_ok)
    {
        if (s_fail_count < s_threshold)
        {
            s_fail_count++;
        }
        if (s_fail_count >= s_threshold)
        {
            s_offline = true;
        }
    }
    else
    {
        if (s_fail_count >= s_threshold)
        {
            s_offline = false;
        }
        s_fail_count = 0;
    }
    return s_offline;
}

void offline_tracker_reset(void)
{
    s_fail_count = 0;
    s_offline = false;
}
