#include "sd_card.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "thermal_display.h"   /* g_lcd_framebuffer */
#include "lcd_ili9341.h"       /* LCD_WIDTH / LCD_HEIGHT */

static const char *TAG = "sd_card";

/** @brief SD card SPI host (uses SPI3_HOST, separate from LCD SPI2_HOST). */
#define SD_HOST SPI3_HOST
/** @brief SD card MISO pin */
#define SD_PIN_NUM_MISO 10
/** @brief SD card CLK pin */
#define SD_PIN_NUM_CLK   11
/** @brief SD card MOSI pin */
#define SD_PIN_NUM_MOSI  12
/** @brief SD card CS pin */
#define SD_PIN_NUM_CS    13

/** @brief True once the SD card has been successfully mounted. */
static bool s_sd_mounted = false;
/** @brief SD card handle (set by mount, used for unmount if needed). */
static sdmmc_card_t *s_sd_card = NULL;

/**
 * @brief Query whether the SD card is currently mounted.
 *
 * @return true if sd_card_init() has succeeded, false otherwise.
 */
bool sd_card_is_mounted(void)
{
    return s_sd_mounted;
}

/**
 * @brief Initialize and mount the microSD card via SPI (SPI3_HOST).
 *
 * Initialises the SPI bus on the SD pins, configures the SDSPI host and
 * device, then mounts the FAT filesystem at SD_MOUNT_POINT. Called lazily
 * on the first capture request so the device boots normally without a card.
 * Safe to call multiple times: after a successful mount this is a no-op.
 *
 * @return
 *   - ESP_OK on success (or already mounted)
 *   - An error code from the SPI / SDSPI / FAT subsystem on failure
 */
esp_err_t sd_card_init(void)
{
    /* No-op if already mounted. */
    if (s_sd_mounted) {
        return ESP_OK;
    }

    esp_err_t ret;

    /* Step 1: initialise the SPI bus on SPI3_HOST with the SD pins. */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_PIN_NUM_MOSI,
        .miso_io_num = SD_PIN_NUM_MISO,
        .sclk_io_num = SD_PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    ret = spi_bus_initialize(SD_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Step 2: configure the SDSPI host (SDSPI_HOST_DEFAULT uses sdspi_host). */
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_HOST;

    /* Step 3: SD SPI device configuration (CS pin, host id, no CD/WP/INT). */
    sdspi_device_config_t dev_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    dev_cfg.host_id  = SD_HOST;
    dev_cfg.gpio_cs  = SD_PIN_NUM_CS;
    dev_cfg.gpio_cd  = SDSPI_SLOT_NO_CD;
    dev_cfg.gpio_wp  = SDSPI_SLOT_NO_WP;
    dev_cfg.gpio_int = GPIO_NUM_NC;

    /* Step 4: FAT mount configuration. */
    esp_vfs_fat_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    /* Step 5: mount the filesystem. ESP-IDF v4.4.2 needs 5 args. */
    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &dev_cfg,
                                   &mount_cfg, &s_sd_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_vfs_fat_sdspi_mount failed: %s", esp_err_to_name(ret));
        spi_bus_free(SD_HOST);
        return ret;
    }

    s_sd_mounted = true;
    ESP_LOGI(TAG, "SD card mounted successfully");
    return ESP_OK;
}

/**
 * @brief Save the current LCD framebuffer as a 24-bit BMP file on SD card.
 *
 * Converts g_lcd_framebuffer (big-endian RGB565, 320x240) to 24-bit BGR and
 * writes a standard uncompressed BMP named "frame_NNNNN.bmp" with an
 * incrementing counter. Rows are written bottom-to-top per the BMP format.
 *
 * @return
 *   - ESP_OK on success
 *   - ESP_ERR_INVALID_STATE if SD not mounted or framebuffer missing
 *   - ESP_ERR_NO_MEM if the row buffer cannot be allocated
 *   - ESP_FAIL on file I/O error
 */
esp_err_t sd_card_save_framebuffer_bmp(void)
{
    if (!s_sd_mounted) {
        ESP_LOGE(TAG, "SD card not mounted — cannot save frame");
        return ESP_ERR_INVALID_STATE;
    }
    if (!g_lcd_framebuffer) {
        ESP_LOGE(TAG, "Framebuffer is NULL — nothing to save");
        return ESP_ERR_INVALID_STATE;
    }

    /* Statically count frames so each capture gets a unique name. */
    static uint32_t s_frame_idx = 0;
    char path[64];
    snprintf(path, sizeof(path), SD_MOUNT_POINT "/frame_%05lu.bmp",
             (unsigned long)s_frame_idx++);

    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s for writing", path);
        return ESP_FAIL;
    }

    /* BMP dimensions. */
    const int w = LCD_WIDTH;   /* 320 */
    const int h = LCD_HEIGHT;  /* 240 */
    const int bpp = 24;
    /* BMP rows are padded to 4-byte boundary. */
    int row_size = ((w * bpp + 31) / 32) * 4;   /* bytes per row */
    int pixel_data_size = row_size * h;
    int file_size = 54 + pixel_data_size;        /* header + info + pixels */

    /* --- BMP file header (14 bytes) --- */
    uint8_t fh[14];
    fh[0] = 'B'; fh[1] = 'M';
    fh[2] = (file_size >>  0) & 0xFF;
    fh[3] = (file_size >>  8) & 0xFF;
    fh[4] = (file_size >> 16) & 0xFF;
    fh[5] = (file_size >> 24) & 0xFF;
    fh[6] = 0; fh[7] = 0;       /* bfReserved1 */
    fh[8] = 0; fh[9] = 0;       /* bfReserved2 */
    fh[10] = 54; fh[11] = 0;    /* bfOffBits */
    fh[12] = 0; fh[13] = 0;     /* bfReserved2 fill */
    fwrite(fh, 1, 14, f);

    /* --- DIB info header (40 bytes) --- */
    uint8_t ih[40];
    memset(ih, 0, sizeof(ih));
    ih[ 0] = 40;                  /* biSize */
    ih[ 4] = (w >>  0) & 0xFF; ih[ 5] = (w >>  8) & 0xFF;
    ih[ 6] = (w >> 16) & 0xFF; ih[ 7] = (w >> 24) & 0xFF;
    ih[ 8] = (h >>  0) & 0xFF; ih[ 9] = (h >>  8) & 0xFF;
    ih[10] = (h >> 16) & 0xFF; ih[11] = (h >> 24) & 0xFF;
    ih[12] = 1; ih[13] = 0;      /* biPlanes = 1 */
    ih[14] = bpp; ih[15] = 0;    /* biBitCount = 24 */
    /* biCompression = 0 (BI_RGB) */
    ih[20] = (pixel_data_size >>  0) & 0xFF;
    ih[21] = (pixel_data_size >>  8) & 0xFF;
    ih[22] = (pixel_data_size >> 16) & 0xFF;
    ih[23] = (pixel_data_size >> 24) & 0xFF;
    fwrite(ih, 1, 40, f);

    /* --- Pixel data: BMP stores rows bottom-to-top and pixels as BGR.
     * g_lcd_framebuffer is big-endian RGB565; we convert to 24-bit BGR
     * and write row-by-row, bottom row first.
     * NOTE: row_buf is only written by the CPU then passed to fwrite();
     * it never touches SPI DMA, so use SPIRAM / default RAM instead of
     * the tiny DMA-capable pool. For 320px width, row_size = 960 bytes. */
    uint8_t *row_buf = (uint8_t *)heap_caps_malloc(row_size, MALLOC_CAP_SPIRAM);
    if (!row_buf)
        row_buf = (uint8_t *)heap_caps_malloc(row_size, MALLOC_CAP_DEFAULT);
    if (!row_buf)
        row_buf = (uint8_t *)malloc(row_size);
    if (!row_buf) {
        ESP_LOGE(TAG, "Row buffer alloc failed (%d bytes)", row_size);
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    for (int y = h - 1; y >= 0; y--) {
        memset(row_buf, 0, row_size);
        for (int x = 0; x < w; x++) {
            /* Framebuffer pixel is big-endian: low byte in memory is high
             * byte of the pixel value. Convert to native order. */
            uint16_t be_pixel = g_lcd_framebuffer[y * LCD_WIDTH + x];
            uint16_t pixel = (be_pixel >> 8) | (be_pixel << 8);
            uint8_t r = (pixel >> 11) & 0x1F;
            uint8_t g = (pixel >> 5)  & 0x3F;
            uint8_t b =  pixel        & 0x1F;
            /* Scale 5-bit channels to 8-bit. */
            r = (r << 3) | (r >> 2);
            g = (g << 2) | (g >> 4);
            b = (b << 3) | (b >> 2);
            int idx = x * 3;
            row_buf[idx + 0] = b;   /* B */
            row_buf[idx + 1] = g;   /* G */
            row_buf[idx + 2] = r;   /* R */
        }
        fwrite(row_buf, 1, row_size, f);
    }

    heap_caps_free(row_buf);
    fclose(f);

    ESP_LOGI(TAG, "Saved frame to %s", path);
    return ESP_OK;
}
