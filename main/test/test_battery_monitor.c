#include "unity.h"
#include "battery_monitor.h"
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static int s_mock_raw_value;

esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t* init,
                               adc_oneshot_unit_handle_t* ret_handle)
{
    (void)init;
    *ret_handle = (adc_oneshot_unit_handle_t)0x1;
    return ESP_OK;
}

esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t handle, adc_channel_t chan,
                                     const adc_oneshot_chan_cfg_t* cfg)
{
    (void)handle;
    (void)chan;
    (void)cfg;
    return ESP_OK;
}

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
esp_err_t adc_cali_create_scheme_curve_fitting(const adc_cali_curve_fitting_config_t* config,
                                               adc_cali_handle_t* ret_handle)
{
    (void)config;
    (void)ret_handle;
    return ESP_FAIL; // Force uncalibrated path
}
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
esp_err_t adc_cali_create_scheme_line_fitting(const adc_cali_line_fitting_config_t* config,
                                              adc_cali_handle_t* ret_handle)
{
    (void)config;
    (void)ret_handle;
    return ESP_FAIL; // Force uncalibrated path
}
#endif

esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t handle, adc_channel_t chan, int* out_raw)
{
    (void)handle;
    (void)chan;
    *out_raw = s_mock_raw_value;
    return ESP_OK;
}

TEST_CASE("battery monitor percent calculation", "[battery]")
{
    battery_monitor_init();

    s_mock_raw_value = 1500; // 3.0V -> 0%
    TEST_ASSERT_EQUAL_UINT8(0, battery_monitor_get_percent());

    s_mock_raw_value = 1800; // 3.6V -> 50%
    TEST_ASSERT_EQUAL_UINT8(50, battery_monitor_get_percent());

    s_mock_raw_value = 2100; // 4.2V -> 100%
    TEST_ASSERT_EQUAL_UINT8(100, battery_monitor_get_percent());
}
