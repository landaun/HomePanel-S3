#include "unity.h"
#include "wifi_manager.h"

// Stub Wi-Fi API to avoid requiring Wi-Fi driver in host tests
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t* ap_info) { return ESP_FAIL; }

TEST_CASE("wifi manager initial status", "[wifi]")
{
    wifi_status_t status = wifi_manager_get_status();
    TEST_ASSERT_EQUAL(WIFI_STATE_DISCONNECTED, status.state);
    TEST_ASSERT_EQUAL_UINT8(0, status.retry_count);
}
