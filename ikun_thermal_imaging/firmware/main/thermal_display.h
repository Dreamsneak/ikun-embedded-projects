#ifndef THERMAL_DISPLAY_H
#define THERMAL_DISPLAY_H

#include <stdint.h>
#include "esp_err.h"
#include "mlx90640.h"
#include "lcd_ili9341.h"

/** @brief Frame buffer for screen capture (RGB565, big-endian).
 *  Allocated in PSRAM for size 320x240x2 = 153600 bytes. */
extern uint16_t *g_lcd_framebuffer;

/**
 * @brief Update a rectangular region of the screen capture framebuffer.
 *
 * Copies pixel data (native RGB565) into the framebuffer for the given
 * rectangle. Called by the LCD driver after every SPI transmit so the
 * framebuffer always mirrors the current panel contents.
 *
 * @param[in] x1 Start X coordinate.
 * @param[in] y1 Start Y coordinate.
 * @param[in] x2 End X coordinate (exclusive).
 * @param[in] y2 End Y coordinate (exclusive).
 * @param[in] data Source pixel buffer (native RGB565, big-endian order
 *                 matching what was sent to the panel).
 */
void framebuf_update_rect(int x1, int y1, int x2, int y2, const uint16_t *data);

/**
 * @defgroup thermal_display Thermal Display Module
 * @brief Module for displaying thermal images on LCD
 *
 * This module provides functions to convert MLX90640 temperature data
 * into visual thermal images and display them on the ILI9341 LCD screen.
 *
 * @{
 */

/** @brief Thermal image zoom factor for display */
#define THERMAL_ZOOM_FACTOR 10

/** @brief Display offset for thermal image */
#define THERMAL_DISPLAY_X_OFFSET 0
#define THERMAL_DISPLAY_Y_OFFSET 0

/** @brief Text overlay reserved area
 *
 * The thermal renderer SKIPS this rectangle entirely (blocks that overlap
 * it transmit only the right-hand part). The overlay text drawn afterwards
 * therefore never gets erased by thermal pixels — no flicker. Height is
 * aligned to the thermal block height (THERMAL_ZOOM_FACTOR) so blocks are
 * either fully inside or fully outside the reserved band. */
#define TEXT_OVERLAY_X 0
#define TEXT_OVERLAY_Y 0
#define TEXT_OVERLAY_W 80
#define TEXT_OVERLAY_H 40

/** @brief Battery icon reserved area (top-right corner)
 *
 * Same skip logic as TEXT_OVERLAY: thermal blocks that overlap this
 * rectangle transmit only the middle band, so the icon is never erased
 * by thermal pixels. Height aligned to block boundary (2 blocks). */
#define BATTERY_OVERLAY_X 290
#define BATTERY_OVERLAY_Y 0
#define BATTERY_OVERLAY_W 30
#define BATTERY_OVERLAY_H 20

/**
 * @brief Initialize the thermal display module.
 *
 * Must be called after LCD initialization.
 *
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t thermal_display_init(void);

/**
 * @brief Display thermal image on LCD.
 *
 * Converts temperature data to colors and displays them on the LCD screen.
 *
 * @param[in] temperatures Array of temperature values from MLX90640 (768 elements).
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t thermal_display_render(const float *temperatures);

/**
 * @brief Draw temperature scale on LCD.
 *
 * Draws a color gradient scale on the right side of the screen.
 *
 * @param[in] min_temp Minimum temperature value.
 * @param[in] max_temp Maximum temperature value.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t thermal_display_draw_scale(float min_temp, float max_temp);

/** @} */

#endif /* THERMAL_DISPLAY_H */
