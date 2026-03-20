#include "unity.h"
#include "scene_service.h"

void mock_ha_client_set_states_json(const char* json);

TEST_CASE("scene_service lists scenes", "[scene]")
{
    const char* json = "[{\"entity_id\":\"scene.movie\",\"attributes\":{\"friendly_name\":"
                       "\"Movie\"}},{\"entity_id\":\"light.kitchen\"}]";
    mock_ha_client_set_states_json(json);
    scene_t scenes[2];
    int count = scene_service_list_scenes(scenes, 2);
    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL_STRING("scene.movie", scenes[0].entity_id);
    TEST_ASSERT_EQUAL_STRING("Movie", scenes[0].name);
}

TEST_CASE("scene_service lists automations", "[scene]")
{
    const char* json = "[{\"entity_id\":\"automation.doorbell\",\"attributes\":{\"friendly_name\":"
                       "\"Doorbell\"},\"state\":\"on\"},{\"entity_id\":\"scene.movie\"}]";
    mock_ha_client_set_states_json(json);
    automation_t autos[2];
    int count = scene_service_list_automations(autos, 2);
    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL_STRING("automation.doorbell", autos[0].entity_id);
    TEST_ASSERT_EQUAL_STRING("Doorbell", autos[0].name);
    TEST_ASSERT_TRUE(autos[0].enabled);
}
