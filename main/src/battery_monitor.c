#include "battery_monitor.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "esp_err.h"

#define BAT_ADC_UNIT ADC_UNIT_1
#define BAT_ADC_CHANNEL ADC_CHANNEL_0
#define BAT_DIVIDER 2.0f
#define BAT_MIN_VOLTAGE 3.0f
#define BAT_MAX_VOLTAGE 4.2f

static const char* TAG = "battery_monitor";

static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t cali_handle;
static bool calibrated = false;

void battery_monitor_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = BAT_ADC_UNIT,
    };
    esp_err_t err = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init ADC unit: %s", esp_err_to_name(err));
        return;
    }

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    err = adc_oneshot_config_channel(adc_handle, BAT_ADC_CHANNEL, &chan_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to config ADC channel: %s", esp_err_to_name(err));
        return;
    }

    /*
     * ADC calibration APIs changed across ESP-IDF releases. Use curve fitting
     * when available, otherwise fall back to line fitting. If neither scheme is
     * supported, continue without calibration.
     */
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = BAT_ADC_UNIT,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle) == ESP_OK)
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = BAT_ADC_UNIT,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle) == ESP_OK)
#else
    if (false)
#endif
    {
        calibrated = true;
    }
    else
    {
        ESP_LOGW(TAG, "ADC calibration failed, using raw values");
    }
}

uint8_t battery_monitor_get_percent(void)
{
    if (!adc_handle)
    {
#if CONFIG_BATTERY_DEBUG
        ESP_LOGW(TAG, "ADC handle not initialized");
#endif
        return 0;
    }

    int raw = 0;
    esp_err_t err = adc_oneshot_read(adc_handle, BAT_ADC_CHANNEL, &raw);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(err));
        return 0;
    }

    int mv = raw;
    if (calibrated)
    {
        err = adc_cali_raw_to_voltage(cali_handle, raw, &mv);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Voltage conversion failed: %s", esp_err_to_name(err));
            return 0;
        }
    }

    float voltage = (float)mv / 1000.0f * BAT_DIVIDER;
    float percent = (voltage - BAT_MIN_VOLTAGE) / (BAT_MAX_VOLTAGE - BAT_MIN_VOLTAGE) * 100.0f;
    if (percent < 0)
    {
        percent = 0;
    }
    else if (percent > 100)
    {
        percent = 100;
    }
#if CONFIG_BATTERY_DEBUG
    ESP_LOGI(TAG, "raw=%d mv=%d voltage=%.2f percent=%.2f", raw, mv, voltage, percent);
#endif
    return (uint8_t)percent;
}
