#include "unity.h"
#include "offline_tracker.h"

TEST_CASE("offline triggers after consecutive failures", "[offline]")
{
    offline_tracker_init(3);
    TEST_ASSERT_FALSE(offline_tracker_update(true));
    offline_tracker_update(false);
    offline_tracker_update(false);
    TEST_ASSERT_TRUE(offline_tracker_update(false));
}

TEST_CASE("offline clears on success", "[offline]")
{
    offline_tracker_init(2);
    offline_tracker_update(false);
    offline_tracker_update(false);
    TEST_ASSERT_TRUE(offline_tracker_update(false));
    TEST_ASSERT_FALSE(offline_tracker_update(true));
}
