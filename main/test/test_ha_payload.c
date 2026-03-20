#include "unity.h"
#include "command_queue.h"
#include "cJSON.h"

TEST_CASE("command_queue builds brightness payload", "[payload]")
{
    TEST_ASSERT_TRUE(command_queue_init());
    TEST_ASSERT_TRUE(command_queue_enqueue_light_brightness("light.kitchen", 123));
    ha_cmd_t cmd;
    TEST_ASSERT_TRUE(command_queue_receive(&cmd));
    TEST_ASSERT_EQUAL(HA_CMD_LIGHT, cmd.type);
    cJSON* brightness = cJSON_GetObjectItem(cmd.data, "brightness");
    TEST_ASSERT_NOT_NULL(brightness);
    TEST_ASSERT_EQUAL_INT(123, brightness->valueint);
    cJSON_Delete(cmd.data);
    command_queue_shutdown();
}

TEST_CASE("command_queue builds color_temp payload", "[payload]")
{
    TEST_ASSERT_TRUE(command_queue_init());
    TEST_ASSERT_TRUE(command_queue_enqueue_light_color_temp("light.desk", 250));
    ha_cmd_t cmd;
    TEST_ASSERT_TRUE(command_queue_receive(&cmd));
    TEST_ASSERT_EQUAL(HA_CMD_LIGHT, cmd.type);
    cJSON* ct = cJSON_GetObjectItem(cmd.data, "color_temp");
    TEST_ASSERT_NOT_NULL(ct);
    TEST_ASSERT_EQUAL_INT(250, ct->valueint);
    cJSON_Delete(cmd.data);
    command_queue_shutdown();
}

TEST_CASE("command_queue builds rgb payload", "[payload]")
{
    TEST_ASSERT_TRUE(command_queue_init());
    TEST_ASSERT_TRUE(command_queue_enqueue_light_rgb("light.strip", 10, 20, 30));
    ha_cmd_t cmd;
    TEST_ASSERT_TRUE(command_queue_receive(&cmd));
    TEST_ASSERT_EQUAL(HA_CMD_LIGHT, cmd.type);
    cJSON* rgb = cJSON_GetObjectItem(cmd.data, "rgb_color");
    TEST_ASSERT_NOT_NULL(rgb);
    TEST_ASSERT_EQUAL_INT(10, cJSON_GetArrayItem(rgb, 0)->valueint);
    TEST_ASSERT_EQUAL_INT(20, cJSON_GetArrayItem(rgb, 1)->valueint);
    TEST_ASSERT_EQUAL_INT(30, cJSON_GetArrayItem(rgb, 2)->valueint);
    cJSON_Delete(cmd.data);
    command_queue_shutdown();
}

TEST_CASE("command_queue builds scene trigger", "[payload]")
{
    TEST_ASSERT_TRUE(command_queue_init());
    TEST_ASSERT_TRUE(command_queue_enqueue_scene("scene.movie"));
    ha_cmd_t cmd;
    TEST_ASSERT_TRUE(command_queue_receive(&cmd));
    TEST_ASSERT_EQUAL(HA_CMD_SCENE, cmd.type);
    TEST_ASSERT_EQUAL_STRING("scene.movie", cmd.entity_id);
    TEST_ASSERT_NULL(cmd.data);
    command_queue_shutdown();
}

TEST_CASE("command_queue builds automation trigger", "[payload]")
{
    TEST_ASSERT_TRUE(command_queue_init());
    TEST_ASSERT_TRUE(command_queue_enqueue_automation("automation.doorbell"));
    ha_cmd_t cmd;
    TEST_ASSERT_TRUE(command_queue_receive(&cmd));
    TEST_ASSERT_EQUAL(HA_CMD_AUTOMATION, cmd.type);
    TEST_ASSERT_EQUAL_STRING("automation.doorbell", cmd.entity_id);
    TEST_ASSERT_NULL(cmd.data);
    command_queue_shutdown();
}
