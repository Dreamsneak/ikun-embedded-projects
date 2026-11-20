#ifndef MLX90640_H
#define MLX90640_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c.h"

/**
 * @defgroup mlx90640 MLX90640 Thermal Sensor Driver
 * @brief Driver for MLX90640 32x24 (768-pixel) thermal imaging sensor
 *
 * Wraps the official Melexis MLX90640 API library (MLX90640_API.c) with an
 * ESP-IDF v4.4 legacy I2C implementation. All register accesses use the
 * correct 16-bit (two-byte) memory addressing required by the sensor.
 *
 * @{
 */

/** @brief MLX90640 I2C address (7-bit) */
#define MLX90640_I2C_ADDR 0x33

/** @brief Sensor geometry (32 columns x 24 rows = 768 pixels) */
#define MLX90640_WIDTH 32
#define MLX90640_HEIGHT 24
#define MLX90640_PIXEL_COUNT (MLX90640_WIDTH * MLX90640_HEIGHT)

/**
 * @brief MLX90640 refresh rate selection (control register 0x800D bits [9:7]).
 *
 * Values follow the datasheet encoding written directly to the register by
 * MLX90640_SetRefreshRate(): 0b000=0.25Hz ... 0b101=8Hz ... 0b111=32Hz.
 * In chess-pattern mode each event is one sub-page; both halves alternate
 * at this rate, so a fully refreshed image appears at the configured rate.
 */
typedef enum {
    MLX90640_RATE_0_25_HZ = 0x00,
    MLX90640_RATE_0_5_HZ = 0x01,
    MLX90640_RATE_1_HZ = 0x02,
    MLX90640_RATE_2_HZ = 0x03,
    MLX90640_RATE_4_HZ = 0x04,
    MLX90640_RATE_8_HZ = 0x05,
    MLX90640_RATE_16_HZ = 0x06,
    MLX90640_RATE_32_HZ = 0x07,
} mlx90640_refresh_rate_t;

/** @brief MLX90640 sensor configuration structure */
typedef struct {
    i2c_port_t i2c_port;       /**< I2C port the sensor is attached to. */
    uint8_t refresh_rate;      /**< mlx90640_refresh_rate_t value. */
    float emissivity;          /**< Scene emissivity (typ. 0.95). */
} mlx90640_config_t;

/**
 * @brief Initialize the MLX90640 sensor.
 *
 * Dumps the 832-word EEPROM, extracts factory calibration parameters with
 * the official Melexis algorithm, and programs the refresh rate via the
 * control register.
 *
 * @param[in] config Sensor configuration parameters.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if config is NULL,
 *         ESP_ERR_INVALID_RESPONSE on I2C or EEPROM-parameter errors.
 */
esp_err_t mlx90640_init(const mlx90640_config_t *config);

/**
 * @brief Read one full frame of temperature data from the sensor.
 *
 * Blocks until the sensor signals a new data frame (up to one refresh
 * period), then converts raw IR data to Celsius using the official
 * Melexis MLX90640_CalculateTo() algorithm (gain, PTAT, offset, kTa/kV
 * compensation included).
 *
 * @param[out] temperatures Array to store temperature values
 *                          (MLX90640_PIXEL_COUNT elements, in Celsius).
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if temperatures
 *         is NULL, ESP_ERR_INVALID_RESPONSE on I2C/frame-data errors.
 */
esp_err_t mlx90640_read_frame(float *temperatures);

/**
 * @brief Get the minimum and maximum temperatures from a frame.
 *
 * @param[in] temperatures Array of temperature values.
 * @param[out] min_temp Pointer to store minimum temperature.
 * @param[out] max_temp Pointer to store maximum temperature.
 */
void mlx90640_get_temp_range(const float *temperatures, float *min_temp, float *max_temp);

/**
 * @brief Convert temperature to RGB565 color for thermal visualization.
 *
 * Uses an ironbow palette (black -> purple -> red -> orange -> yellow ->
 * white) with a smoothstep midtone-contrast curve. The temperature is
 * normalized to [min_temp, max_temp] and clamped at both ends.
 *
 * @param[in] temp Temperature value in Celsius.
 * @param[in] min_temp Minimum temperature for normalization.
 * @param[in] max_temp Maximum temperature for normalization.
 * @return uint16_t RGB565 color value.
 */
uint16_t mlx90640_temp_to_color(float temp, float min_temp, float max_temp);

/** @} */

#endif /* MLX90640_H */
