#include "battery_adc.h"
#include "driver/adc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/** @brief Flag indicating whether ADC has been initialized */
static bool s_adc_initialized = false;

/**
 * @brief Initialize the battery ADC subsystem.
 *
 * Configures ADC1 with the specified resolution width and channel attenuation.
 * This function must be called before any other battery ADC functions.
 *
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if parameter error,
 *         or other error codes from adc1_config_width or adc1_config_channel_atten.
 */
esp_err_t battery_adc_init(void)
{
    esp_err_t ret;

    ret = adc1_config_width(BATTERY_ADC_WIDTH);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = adc1_config_channel_atten(BATTERY_ADC_CHANNEL, BATTERY_ADC_ATTEN);
    if (ret != ESP_OK) {
        return ret;
    }

    s_adc_initialized = true;
    return ESP_OK;
}

/**
 * @brief Read raw ADC value from battery channel.
 *
 * Returns the raw 12-bit ADC value from the configured battery channel.
 * If the ADC has not been initialized, returns 0.
 *
 * @return uint32_t Raw ADC value (0-4095), or 0 if not initialized.
 */
uint32_t battery_adc_read_raw(void)
{
    if (!s_adc_initialized) {
        return 0;
    }

    return adc1_get_raw(BATTERY_ADC_CHANNEL);
}

/**
 * @brief Read ADC input voltage in millivolts.
 *
 * Calculates the voltage at the ADC input pin using the formula:
 * voltage = raw_value * full_scale_voltage / max_adc_value
 *
 * @return uint32_t ADC input voltage in millivolts (0-3900mV).
 */
uint32_t battery_adc_read_voltage(void)
{
    uint32_t raw = battery_adc_read_raw();
    return (raw * ADC_ATTEN_DB11_FULL_SCALE) / ADC_MAX_VALUE;
}

/**
 * @brief Read actual battery voltage in millivolts.
 *
 * Calculates the actual battery voltage by applying the voltage divider ratio:
 * battery_voltage = adc_voltage * (R12 + R11) / R11
 *
 * @return uint32_t Battery voltage in millivolts.
 */
uint32_t battery_adc_read_battery_voltage(void)
{
    uint32_t adc_voltage = battery_adc_read_voltage();
    return (uint32_t)(adc_voltage * BATTERY_DIVIDER_RATIO);
}

/**
 * @brief Calculate battery level percentage.
 *
 * Computes the battery level based on the actual battery voltage using linear interpolation:
 * - 4200mV (4.2V) = 100%
 * - 3000mV (3.0V) = 0%
 *
 * @return int Battery level percentage (0-100).
 */
int battery_adc_read_level(void)
{
    uint32_t battery_voltage = battery_adc_read_battery_voltage();

    if (battery_voltage >= BATTERY_MAX_VOLTAGE) {
        return 100;
    } else if (battery_voltage <= BATTERY_MIN_VOLTAGE) {
        return 0;
    } else {
        return (int)(((battery_voltage - BATTERY_MIN_VOLTAGE) * 100) / 
                      (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE));
    }
}
