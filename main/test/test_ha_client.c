#include "unity.h"
#include "ha_client.h"
#include "command_queue.h"
#include "cJSON.h"

TEST_CASE("ha_client requires initialization", "[ha_client]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ha_client_connect());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ha_client_disconnect());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ha_client_subscribe("light.test"));
}

TEST_CASE("ha_client_put requires init and path", "[ha_client]")
{
    ha_client_config_t cfg = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ha_client_put("/api/test", NULL));
    TEST_ASSERT_EQUAL(ESP_OK, ha_client_init(&cfg));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ha_client_put(NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, ha_client_put("/api/test", NULL));
    ha_client_deinit();
}

TEST_CASE("ha_client init/deinit lifecycle", "[ha_client]")
{
    ha_client_config_t cfg = {0};
    TEST_ASSERT_EQUAL(ESP_OK, ha_client_init(&cfg));
    ha_client_deinit();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ha_client_connect());
    TEST_ASSERT_EQUAL(ESP_OK, ha_client_init(&cfg));
    ha_client_deinit();
}

TEST_CASE("command_queue_enqueue_put stores PUT request", "[command_queue]")
{
    TEST_ASSERT_TRUE(command_queue_init());
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "n", 1);
    TEST_ASSERT_TRUE(command_queue_enqueue_put("/api/x", obj));
    cJSON_Delete(obj);
    ha_cmd_t cmd;
    TEST_ASSERT_TRUE(command_queue_receive(&cmd));
    TEST_ASSERT_EQUAL(HA_CMD_REQUEST, cmd.type);
    TEST_ASSERT_EQUAL_STRING("PUT", cmd.method);
    TEST_ASSERT_EQUAL_STRING("/api/x", cmd.path);
    cJSON_Delete(cmd.data);
    command_queue_shutdown();
}
