idf_component_register(SRCS "test_config.c" "test_wifi_manager.c" "test_ha_client.c" "test_ha_payload.c" "test_scene_service.c"
"test_offline_tracker.c" "mock_ha_client.c" "test_battery_monitor.c"
                       PRIV_REQUIRES unity main nvs_flash)

# Compile main component with UNIT_TEST to replace networked HA client
idf_component_get_property(main_lib main COMPONENT_LIB)
target_compile_definitions(${main_lib} PUBLIC UNIT_TEST)
