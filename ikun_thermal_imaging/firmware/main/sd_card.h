#ifndef SD_CARD_H
#define SD_CARD_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @defgroup sd_card SD Card Module
 * @brief microSD card storage over SPI (SPI3_HOST, separate from LCD SPI2_HOST)
 *
 * Provides lazy mounting and 24-bit BMP screen capture of the LCD
 * framebuffer. The mount is performed on demand by @ref sd_card_init();
 * @ref sd_card_is_mounted() lets callers avoid re-mounting.
 *
 * Pin assignments (defined in sd_card.c):
 *   - MISO: GPIO10
 *   - CLK : GPIO11
 *   - MOSI: GPIO12
 *   - CS  : GPIO13
 *
 * @{
 */

/** @brief VFS mount point for the SD card FAT filesystem. */
#define SD_MOUNT_POINT "/sdcard"

/**
 * @brief Initialize and mount the microSD card via SPI.
 *
 * Uses SPI3_HOST on the pins defined in sd_card.c. The SPI bus is
 * explicitly initialised first, then esp_vfs_fat_sdspi_mount() is
 * called with a correctly populated sdspi_device_config_t for the
 * ESP-IDF version in use.
 *
 * Safe to call multiple times: subsequent calls after a successful
 * mount are no-ops and return ESP_OK.
 *
 * @return
 *   - ESP_OK on success (or already mounted)
 *   - An error code from the SPI / SDSPI / FAT subsystem on failure
 */
esp_err_t sd_card_init(void);

/**
 * @brief Query whether the SD card is currently mounted.
 *
 * @return true if @ref sd_card_init() has succeeded, false otherwise.
 */
bool sd_card_is_mounted(void);

/**
 * @brief Save the current LCD framebuffer as a 24-bit BMP file on SD card.
 *
 * Reads the global @ref g_lcd_framebuffer (big-endian RGB565, 320x240)
 * and writes it as a standard 24-bit uncompressed BMP file. The file is
 * named "/sdcard/frame_NNNNN.bmp" where NNNNN is an incrementing counter
 * so multiple captures do not overwrite each other.
 *
 * @return
 *   - ESP_OK on success
 *   - ESP_ERR_INVALID_STATE if SD not mounted or framebuffer missing
 *   - ESP_ERR_NO_MEM if the row buffer cannot be allocated
 *   - ESP_FAIL on file I/O error
 */
esp_err_t sd_card_save_framebuffer_bmp(void);

/** @} */

#endif /* SD_CARD_H */
