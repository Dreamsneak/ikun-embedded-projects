#ifndef BATTERY_ADC_H
#define BATTERY_ADC_H

#include <stdint.h>
#include "esp_err.h"

/**
 * @defgroup battery_adc Battery ADC Driver
 * @brief Driver for battery voltage monitoring using ADC
 *
 * This driver provides functions to initialize and read battery voltage
 * using ESP32-S3 ADC. It supports voltage calculation with voltage divider
 * ratio and battery level percentage calculation.
 *
 * @{
 */

/** @brief ADC channel for battery monitoring (IO4 = ADC1_CH3) */
#define BATTERY_ADC_CHANNEL ADC1_CHANNEL_3

/** @brief ADC attenuation for battery input (11dB = 0-3.9V range) */
#define BATTERY_ADC_ATTEN ADC_ATTEN_DB_11

/** @brief ADC resolution (12-bit = 4096 levels) */
#define BATTERY_ADC_WIDTH ADC_WIDTH_BIT_12

/** @brief Voltage divider ratio: (R12 + R11) / R11 = (5.1K + 10K) / 10K */
#define BATTERY_DIVIDER_RATIO 1.51f

/** @brief Maximum battery voltage (4.2V = 100%) */
#define BATTERY_MAX_VOLTAGE 4200

/** @brief Minimum battery voltage (3.0V = 0%) */
#define BATTERY_MIN_VOLTAGE 3000

/** @brief ADC full scale voltage for 11dB attenuation (3900mV) */
#define ADC_ATTEN_DB11_FULL_SCALE 3900

/** @brief ADC maximum value for 12-bit resolution */
#define ADC_MAX_VALUE 4095

/**
 * @brief Initialize the battery ADC subsystem.
 *
 * Configures ADC1 with the specified width and channel attenuation.
 *
 * @return esp_err_t ESP_OK on success, or error code on failure.
 */
esp_err_t battery_adc_init(void);

/**
 * @brief Read raw ADC value from battery channel.
 *
 * @return uint32_t Raw ADC value (0-4095).
 */
uint32_t battery_adc_read_raw(void);

/**
 * @brief Read ADC input voltage in millivolts.
 *
 * Calculates the voltage at the ADC input pin (after voltage divider).
 *
 * @return uint32_t ADC input voltage in millivolts.
 */
uint32_t battery_adc_read_voltage(void);

/**
 * @brief Read actual battery voltage in millivolts.
 *
 * Calculates the actual battery voltage by applying the voltage divider ratio.
 *
 * @return uint32_t Battery voltage in millivolts.
 */
uint32_t battery_adc_read_battery_voltage(void);

/**
 * @brief Calculate battery level percentage.
 *
 * Computes the battery level based on the actual battery voltage,
 * with 4.2V = 100% and 3.0V = 0%.
 *
 * @return int Battery level percentage (0-100).
 */
int battery_adc_read_level(void);

/** @} */

#endif /* BATTERY_ADC_H */
