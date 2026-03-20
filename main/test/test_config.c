#include "unity.h"
#include "config.h"
#include <stdlib.h>

TEST_CASE("config save and load roundtrip", "[config]")
{
    panel_config_t cfg = {.wifi_ssid = "ssid",
                          .wifi_password = "pass",
                          .ha_url = "http://example.com",
                          .ha_token = "Bearer 123",
                          .use_https = true,
                          .update_interval_ms = 5000};
    panel_config_t loaded;

    TEST_ASSERT_EQUAL(ESP_OK, config_init());
    TEST_ASSERT_EQUAL(ESP_OK, config_save(&cfg));
    TEST_ASSERT_EQUAL(ESP_OK, config_load(&loaded));

    TEST_ASSERT_EQUAL_STRING("ssid", loaded.wifi_ssid);
    TEST_ASSERT_EQUAL_STRING("pass", loaded.wifi_password);
    TEST_ASSERT_EQUAL_STRING("http://example.com", loaded.ha_url);
    TEST_ASSERT_EQUAL_STRING("Bearer 123", loaded.ha_token);
    TEST_ASSERT_TRUE(loaded.use_https);
    TEST_ASSERT_EQUAL(5000, loaded.update_interval_ms);

    TEST_ASSERT_EQUAL(ESP_OK, config_reset_to_defaults());
}

TEST_CASE("config loads from environment variables", "[config]")
{
    panel_config_t cfg;

    setenv("WIFI_SSID", "env_ssid", 1);
    setenv("WIFI_PASSWORD", "env_pass", 1);
    setenv("HA_BASE_URL", "http://env.example", 1);
    setenv("HA_TOKEN", "Bearer env", 1);

    TEST_ASSERT_EQUAL(ESP_OK, config_init());
    TEST_ASSERT_EQUAL(ESP_OK, config_load(&cfg));

    TEST_ASSERT_EQUAL_STRING("env_ssid", cfg.wifi_ssid);
    TEST_ASSERT_EQUAL_STRING("env_pass", cfg.wifi_password);
    TEST_ASSERT_EQUAL_STRING("", cfg.ha_url);
    TEST_ASSERT_EQUAL_STRING("", cfg.ha_token);

    unsetenv("WIFI_SSID");
    unsetenv("WIFI_PASSWORD");
    unsetenv("HA_BASE_URL");
    unsetenv("HA_TOKEN");

    TEST_ASSERT_EQUAL(ESP_OK, config_reset_to_defaults());
}
