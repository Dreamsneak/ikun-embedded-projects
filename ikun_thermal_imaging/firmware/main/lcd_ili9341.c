#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "lcd_ili9341.h"
#include "thermal_display.h"
#include "font.h"

static const char *TAG = "ili9341";

/** @brief SPI device handle for LCD communication. */
static spi_device_handle_t s_spi = NULL;

/**
 * @brief Send a command byte to the LCD (DC=low).
 *
 * @param[in] cmd 8-bit command register address.
 */
static void lcd_wr_cmd(uint8_t cmd)
{
    gpio_set_level(PIN_NUM_CS, 0);
    gpio_set_level(PIN_NUM_DC, 0);          /* command mode */
    spi_transaction_t t = {
        .length = 8,
        .tx_data = { cmd },
        .flags = SPI_TRANS_USE_TXDATA,
    };
    spi_device_polling_transmit(s_spi, &t);
    gpio_set_level(PIN_NUM_CS, 1);
}

/**
 * @brief Send a command followed by multiple data bytes in one CS cycle.
 *
 * Equivalent to CH32's LCD_WR_REG + LCD_WR_DATA sequence but keeps CS low
 * across the whole transfer for efficiency.
 *
 * @param[in] cmd Command byte.
 * @param[in] data Pointer to data bytes.
 * @param[in] len Number of data bytes.
 */
static void lcd_wr_cmd_data(uint8_t cmd, const uint8_t *data, int len)
{
    gpio_set_level(PIN_NUM_CS, 0);
    gpio_set_level(PIN_NUM_DC, 0);          /* command */
    spi_transaction_t tc = {
        .length = 8,
        .tx_data = { cmd },
        .flags = SPI_TRANS_USE_TXDATA,
    };
    spi_device_polling_transmit(s_spi, &tc);

    gpio_set_level(PIN_NUM_DC, 1);          /* data */
    for (int i = 0; i < len; i++) {
        spi_transaction_t td = {
            .length = 8,
            .tx_data = { data[i] },
            .flags = SPI_TRANS_USE_TXDATA,
        };
        spi_device_polling_transmit(s_spi, &td);
    }
    gpio_set_level(PIN_NUM_CS, 1);
}

/**
 * @brief Set the LCD drawing window (column + row address + write RAM).
 *
 * Mirrors CH32 LCD_SetWindows(). Coordinates are inclusive (x2/y2 are the
 * last pixel, not one-past-the-end).
 *
 * @param[in] x1 Start X.
 * @param[in] y1 Start Y.
 * @param[in] x2 End X (inclusive).
 * @param[in] y2 End Y (inclusive).
 */
static void lcd_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    uint8_t x_cmd[4] = { (x1 >> 8) & 0xFF, x1 & 0xFF, (x2 >> 8) & 0xFF, x2 & 0xFF };
    uint8_t y_cmd[4] = { (y1 >> 8) & 0xFF, y1 & 0xFF, (y2 >> 8) & 0xFF, y2 & 0xFF };
    lcd_wr_cmd_data(0x2A, x_cmd, 4);        /* column address set */
    lcd_wr_cmd_data(0x2B, y_cmd, 4);        /* row address set */
    lcd_wr_cmd(0x2C);                        /* memory write */
}

/**
 * @brief Initialize the SPI bus and add the LCD device.
 */
static void lcd_spi_init(void)
{
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000,  /* 40 MHz */
        .mode = 0,                            /* CPOL=0, CPHA=0 (Mode 0) */
        .spics_io_num = -1,                   /* CS controlled manually via GPIO */
        .queue_size = 1,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(LCD_HOST, &devcfg, &s_spi));
}

/**
 * @brief Configure DC/CS/RST/BCKL GPIOs as outputs.
 */
static void lcd_gpio_init(void)
{
    uint64_t pin_mask = (1ULL << PIN_NUM_DC) |
                        (1ULL << PIN_NUM_CS) |
                        (1ULL << PIN_NUM_RST) |
                        (1ULL << PIN_NUM_BCKL);
    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    /* Idle states: CS high (deselected), DC high (data), BL on. */
    gpio_set_level(PIN_NUM_CS, 1);
    gpio_set_level(PIN_NUM_DC, 1);
    gpio_set_level(PIN_NUM_BCKL, 1);
}

/**
 * @brief Hardware reset the LCD via the RST pin.
 *
 * Mirrors CH32 LCD_RESET(): RST low 100ms, high 50ms.
 */
static void lcd_reset(void)
{
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

/**
 * @brief Initialize the LCD display subsystem.
 *
 * Initializes SPI, GPIOs, performs reset, sends the complete ILI9341 init
 * sequence (copied verbatim from the verified CH32 LCD_Init), and turns on
 * the backlight. MADCTL=0x68 sets BGR=1 + MX + MV for landscape orientation
 * with correct sub-pixel mapping.
 */
void lcd_init(void)
{
    lcd_spi_init();
    lcd_gpio_init();
    lcd_reset();
    gpio_set_level(PIN_NUM_BCKL, 1);        /* backlight on */

    /*
     * Init sequence copied from the user's verified CH32 LCD_Init().
     * Register values and order must match exactly — do NOT substitute
     * values from other vendor libraries.
     */
    lcd_wr_cmd_data(0xCF, (uint8_t[]) {0x00, 0xC9, 0x30}, 3);
    lcd_wr_cmd_data(0xED, (uint8_t[]) {0x64, 0x03, 0x12, 0x81}, 4);
    lcd_wr_cmd_data(0xE8, (uint8_t[]) {0x85, 0x10, 0x7A}, 3);
    lcd_wr_cmd_data(0xCB, (uint8_t[]) {0x39, 0x2C, 0x00, 0x34, 0x02}, 5);
    lcd_wr_cmd_data(0xF7, (uint8_t[]) {0x20}, 1);
    lcd_wr_cmd_data(0xEA, (uint8_t[]) {0x00, 0x00}, 2);

    lcd_wr_cmd_data(0xC0, (uint8_t[]) {0x1B}, 1);       /* VRH[5:0] */
    lcd_wr_cmd_data(0xC1, (uint8_t[]) {0x00}, 1);       /* SAP[2:0];BT[3:0] */
    lcd_wr_cmd_data(0xC5, (uint8_t[]) {0x30, 0x30}, 2); /* VCM control */
    lcd_wr_cmd_data(0xC7, (uint8_t[]) {0xB7}, 1);       /* VCM control2 */

    lcd_wr_cmd_data(0x3A, (uint8_t[]) {0x55}, 1);       /* COLMOD 16-bit */
    lcd_wr_cmd_data(0xB1, (uint8_t[]) {0x00, 0x1A}, 2); /* Frame rate */
    lcd_wr_cmd_data(0xB6, (uint8_t[]) {0x0A, 0xA2}, 2); /* Display Function Control */

    lcd_wr_cmd_data(0xF2, (uint8_t[]) {0x00}, 1);       /* 3Gamma disable */
    lcd_wr_cmd_data(0x26, (uint8_t[]) {0x01}, 1);       /* Gamma curve 1 */

    /* Positive gamma (verified values from CH32 code). */
    lcd_wr_cmd_data(0xE0, (uint8_t[]) {
        0x0F, 0x2A, 0x28, 0x08, 0x0E, 0x08, 0x54, 0xA9,
        0x43, 0x0A, 0x0F, 0x00, 0x00, 0x00, 0x00
    }, 15);
    /* Negative gamma (verified values from CH32 code). */
    lcd_wr_cmd_data(0xE1, (uint8_t[]) {
        0x00, 0x15, 0x17, 0x07, 0x11, 0x06, 0x2B, 0x56,
        0x3C, 0x05, 0x10, 0x0F, 0x3F, 0x3F, 0x0F
    }, 15);

    /* Row/column address range for the initial frame. */
    lcd_wr_cmd_data(0x2B, (uint8_t[]) {0x00, 0x00, 0x01, 0x3F}, 4);
    lcd_wr_cmd_data(0x2A, (uint8_t[]) {0x00, 0x00, 0x00, 0xEF}, 4);

    lcd_wr_cmd(0x11);                        /* Exit Sleep */
    vTaskDelay(pdMS_TO_TICKS(120));

    /* MADCTL=0x68: BGR=1, MX=1, MV=1, MY=0 (landscape, sub-pixel mapped). */
    // lcd_wr_cmd_data(0x36, (uint8_t[]) {0x68}, 1);
    lcd_wr_cmd_data(0x36, (uint8_t[]) {0xA8}, 1);

    lcd_wr_cmd(0x29);                        /* Display ON */

    ESP_LOGI(TAG, "ILI9341 initialized (direct SPI, MADCTL=0xA8, BGR=1)");
}

/**
 * @brief Fill the entire screen with a single color.
 *
 * @param[in] color RGB565 color value.
 */
void fill_screen(uint16_t color)
{
    int buf_size = LCD_WIDTH * sizeof(uint16_t);
    uint16_t *buf = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    if (!buf) {
        ESP_LOGE(TAG, "fill_screen: malloc failed");
        return;
    }

    /* Store pixels in big-endian so SPI transmits high byte first. */
    uint16_t be = (color >> 8) | (color << 8);
    for (int i = 0; i < LCD_WIDTH; i++) {
        buf[i] = be;
    }

    lcd_set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

    gpio_set_level(PIN_NUM_CS, 0);
    gpio_set_level(PIN_NUM_DC, 1);          /* data mode */
    spi_transaction_t t = {
        .length = LCD_WIDTH * 16,
        .tx_buffer = buf,
    };
    for (int y = 0; y < LCD_HEIGHT; y++) {
        spi_device_polling_transmit(s_spi, &t);
    }
    gpio_set_level(PIN_NUM_CS, 1);

    /* Update the framebuffer directly — single color fill, avoid
     * framebuf_update_rect() which expects a full pixel buffer, not
     * just a pointer to one color (would cause OOB reads). */
    if (g_lcd_framebuffer) {
        uint16_t be = (color >> 8) | (color << 8);
        for (int y = 0; y < LCD_HEIGHT; y++) {
            for (int x = 0; x < LCD_WIDTH; x++) {
                g_lcd_framebuffer[y * LCD_WIDTH + x] = be;
            }
        }
    }

    heap_caps_free(buf);
}

/**
 * @brief Fill a rectangular area with a single color.
 *
 * @param[in] x1 Starting X coordinate.
 * @param[in] y1 Starting Y coordinate.
 * @param[in] x2 Ending X coordinate (exclusive).
 * @param[in] y2 Ending Y coordinate (exclusive).
 * @param[in] color RGB565 color value.
 */
void fill_rect(int x1, int y1, int x2, int y2, uint16_t color)
{
    int width = x2 - x1;
    if (width <= 0 || y2 <= y1) {
        return;
    }
    int buf_size = width * sizeof(uint16_t);
    uint16_t *buf = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    if (!buf) {
        ESP_LOGE(TAG, "fill_rect: malloc failed");
        return;
    }

    uint16_t be = (color >> 8) | (color << 8);
    for (int i = 0; i < width; i++) {
        buf[i] = be;
    }

    lcd_set_window(x1, y1, x2 - 1, y2 - 1);

    gpio_set_level(PIN_NUM_CS, 0);
    gpio_set_level(PIN_NUM_DC, 1);
    spi_transaction_t t = {
        .length = width * 16,
        .tx_buffer = buf,
    };
    int height = y2 - y1;
    for (int y = 0; y < height; y++) {
        spi_device_polling_transmit(s_spi, &t);
    }
    gpio_set_level(PIN_NUM_CS, 1);

    /* Update the framebuffer directly — single color fill. */
    if (g_lcd_framebuffer) {
        for (int y = y1; y < y2; y++) {
            if (y < 0 || y >= LCD_HEIGHT) continue;
            for (int x = x1; x < x2; x++) {
                if (x < 0 || x >= LCD_WIDTH) continue;
                g_lcd_framebuffer[y * LCD_WIDTH + x] = be;
            }
        }
    }

    heap_caps_free(buf);
}

/**
 * @brief Draw a bitmap of RGB565 color data to a rectangular area.
 *
 * The input buffer is standard RGB565 (little-endian in memory). This
 * function converts each pixel to big-endian before sending so the panel
 * receives the high byte first.
 *
 * @param[in] x1 Starting X coordinate.
 * @param[in] y1 Starting Y coordinate.
 * @param[in] x2 Ending X coordinate (exclusive).
 * @param[in] y2 Ending Y coordinate (exclusive).
 * @param[in] color_data Pointer to the RGB565 color data buffer.
 */
void lcd_draw_bitmap(int x1, int y1, int x2, int y2, const uint16_t *color_data)
{
    int width = x2 - x1;
    int height = y2 - y1;
    if (width <= 0 || height <= 0) {
        return;
    }

    int num_pixels = width * height;
    int buf_size = num_pixels * sizeof(uint16_t);
    uint16_t *buf = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    if (!buf) {
        ESP_LOGE(TAG, "lcd_draw_bitmap: malloc %d failed", buf_size);
        return;
    }

    /* Convert little-endian RGB565 to big-endian for SPI transmission. */
    for (int i = 0; i < num_pixels; i++) {
        uint16_t c = color_data[i];
        buf[i] = (c >> 8) | (c << 8);
    }

    lcd_set_window(x1, y1, x2 - 1, y2 - 1);

    gpio_set_level(PIN_NUM_CS, 0);
    gpio_set_level(PIN_NUM_DC, 1);
    spi_transaction_t t = {
        .length = num_pixels * 16,
        .tx_buffer = buf,
    };
    spi_device_polling_transmit(s_spi, &t);
    gpio_set_level(PIN_NUM_CS, 1);

    /* Update the framebuffer for screen capture (native-endian input). */
    framebuf_update_rect(x1, y1, x2, y2, color_data);

    heap_caps_free(buf);
}

/**
 * @brief Draw a 16-bit image exported by Image2Lcd.
 *
 * Renders a color bitmap from Image2Lcd v2.9 (vertical scan, 16-bit true
 * color, optional 8-byte header). Each parsed pixel is converted to
 * big-endian and sent to the panel.
 *
 * @param[in] x Starting X coordinate.
 * @param[in] y Starting Y coordinate.
 * @param[in] image_data Pointer to the Image2Lcd exported C array.
 * @param[in] w Image width (fallback when header absent).
 * @param[in] h Image height (fallback when header absent).
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG on invalid input.
 */
esp_err_t lcd_draw_image2lcd(int x, int y, const uint8_t *image_data,
                             int w, int h)
{
    if (!image_data) {
        ESP_LOGE(TAG, "image_data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    /* Detect 8-byte Image2Lcd header. */
    int hdr_w = image_data[2] | (image_data[3] << 8);
    int hdr_h = image_data[4] | (image_data[5] << 8);
    int img_w, img_h, data_offset;

    if (image_data[1] == 0x10 && hdr_w > 0 && hdr_w <= 1024 &&
        hdr_h > 0 && hdr_h <= 1024) {
        img_w = hdr_w;
        img_h = hdr_h;
        data_offset = 8;
    } else {
        img_w = w;
        img_h = h;
        data_offset = 0;
    }

    if (img_w <= 0 || img_h <= 0) {
        ESP_LOGE(TAG, "invalid image dimensions: %dx%d", img_w, img_h);
        return ESP_ERR_INVALID_ARG;
    }

    /* Clamp to screen. */
    if (x + img_w > LCD_WIDTH) {
        img_w = LCD_WIDTH - x;
    }
    if (y + img_h > LCD_HEIGHT) {
        img_h = LCD_HEIGHT - y;
    }
    if (img_w <= 0 || img_h <= 0) {
        return ESP_OK;
    }

    /* Allocate big-endian frame buffer for the panel. */
    uint32_t frame_size = (uint32_t)img_w * (uint32_t)img_h * sizeof(uint16_t);
    uint16_t *frame_buf = heap_caps_malloc(frame_size, MALLOC_CAP_DMA);
    if (!frame_buf) {
        ESP_LOGE(TAG, "image frame buffer alloc %lu failed",
                 (unsigned long)frame_size);
        return ESP_ERR_NO_MEM;
    }

    const uint8_t *pdata = image_data + data_offset;

    /*
     * Parse image data exactly like the CH32 Showimage(): horizontal scan
     * (row * width + col), little-endian pixels (low byte first), no channel
     * swap. The BGR=1 bit in MADCTL handles sub-pixel mapping, so raw
     * RGB565 data is sent as-is.
     */
    for (int row = 0; row < img_h; row++) {
        for (int col = 0; col < img_w; col++) {
            uint32_t src_byte = ((uint32_t)row * (uint32_t)img_w + (uint32_t)col) * 2;
            uint16_t pixel = pdata[src_byte] | ((uint16_t)pdata[src_byte + 1] << 8);
            /* Rotate 90° left (CCW): src(row,col) -> dst(img_w-1-col, row). */
            frame_buf[(img_w - 1 - col) * img_w + row] = (pixel >> 8) | (pixel << 8);
        }
    }

    lcd_set_window(x, y, x + img_w - 1, y + img_h - 1);

    gpio_set_level(PIN_NUM_CS, 0);
    gpio_set_level(PIN_NUM_DC, 1);
    spi_transaction_t t = {
        .length = (size_t)img_w * img_h * 16,
        .tx_buffer = frame_buf,
    };
    spi_device_polling_transmit(s_spi, &t);
    gpio_set_level(PIN_NUM_CS, 1);

    /* Update framebuffer: frame_buf is big-endian. */
    if (g_lcd_framebuffer) {
        for (int row = 0; row < img_h; row++) {
            int dst_y = y + row;
            if (dst_y < 0 || dst_y >= LCD_HEIGHT) continue;
            for (int col = 0; col < img_w; col++) {
                int dst_x = x + col;
                if (dst_x < 0 || dst_x >= LCD_WIDTH) continue;
                g_lcd_framebuffer[dst_y * LCD_WIDTH + dst_x] =
                    frame_buf[row * img_w + col];
            }
        }
    }

    heap_caps_free(frame_buf);
    return ESP_OK;
}

/**
 * @brief Draw a 16-bit Image2Lcd bitmap flipped 180 degrees on screen.
 *
 * Same data parsing as lcd_draw_image2lcd() (horizontal scan, little-endian
 * RGB565, auto-detected 8-byte header). The standard renderer rotates the
 * source 90 degrees CCW for landscape display; this variant rotates it a
 * further 180 degrees (i.e. 90 degrees CW relative to the source), so the
 * on-screen result is upside down versus lcd_draw_image2lcd(). Intended for
 * square images (no dimension swap).
 *
 * @param[in] x Starting X coordinate.
 * @param[in] y Starting Y coordinate.
 * @param[in] image_data Pointer to the Image2Lcd exported C array.
 * @param[in] w Image width (fallback when header absent).
 * @param[in] h Image height (fallback when header absent).
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG on invalid input.
 */
esp_err_t lcd_draw_image2lcd_180(int x, int y, const uint8_t *image_data,
                                 int w, int h)
{
    if (!image_data) {
        ESP_LOGE(TAG, "image_data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    /* Detect 8-byte Image2Lcd header. */
    int hdr_w = image_data[2] | (image_data[3] << 8);
    int hdr_h = image_data[4] | (image_data[5] << 8);
    int img_w, img_h, data_offset;

    if (image_data[1] == 0x10 && hdr_w > 0 && hdr_w <= 1024 &&
        hdr_h > 0 && hdr_h <= 1024) {
        img_w = hdr_w;
        img_h = hdr_h;
        data_offset = 8;
    } else {
        img_w = w;
        img_h = h;
        data_offset = 0;
    }

    if (img_w <= 0 || img_h <= 0) {
        ESP_LOGE(TAG, "invalid image dimensions: %dx%d", img_w, img_h);
        return ESP_ERR_INVALID_ARG;
    }

    /* Clamp to screen. */
    if (x + img_w > LCD_WIDTH) {
        img_w = LCD_WIDTH - x;
    }
    if (y + img_h > LCD_HEIGHT) {
        img_h = LCD_HEIGHT - y;
    }
    if (img_w <= 0 || img_h <= 0) {
        return ESP_OK;
    }

    /* Allocate big-endian frame buffer for the panel. */
    uint32_t frame_size = (uint32_t)img_w * (uint32_t)img_h * sizeof(uint16_t);
    uint16_t *frame_buf = heap_caps_malloc(frame_size, MALLOC_CAP_DMA);
    if (!frame_buf) {
        ESP_LOGE(TAG, "image frame buffer alloc %lu failed",
                 (unsigned long)frame_size);
        return ESP_ERR_NO_MEM;
    }

    const uint8_t *pdata = image_data + data_offset;

    /* Parse RGB565 little-endian pixels. Standard mapping is 90° CCW
     * dst(w-1-col, row); adding 180° gives 90° CW: src(row,col) ->
     * dst(col, h-1-row). */
    for (int row = 0; row < img_h; row++) {
        for (int col = 0; col < img_w; col++) {
            uint32_t src_byte = ((uint32_t)row * (uint32_t)img_w + (uint32_t)col) * 2;
            uint16_t pixel = pdata[src_byte] | ((uint16_t)pdata[src_byte + 1] << 8);
            int dst_idx = col * img_w + (img_h - 1 - row);
            frame_buf[dst_idx] = (pixel >> 8) | (pixel << 8);
        }
    }

    lcd_set_window(x, y, x + img_w - 1, y + img_h - 1);

    gpio_set_level(PIN_NUM_CS, 0);
    gpio_set_level(PIN_NUM_DC, 1);
    spi_transaction_t t = {
        .length = (size_t)img_w * img_h * 16,
        .tx_buffer = frame_buf,
    };
    spi_device_polling_transmit(s_spi, &t);
    gpio_set_level(PIN_NUM_CS, 1);

    /* Update framebuffer: frame_buf is big-endian. */
    if (g_lcd_framebuffer) {
        for (int row = 0; row < img_h; row++) {
            int dst_y = y + row;
            if (dst_y < 0 || dst_y >= LCD_HEIGHT) continue;
            for (int col = 0; col < img_w; col++) {
                int dst_x = x + col;
                if (dst_x < 0 || dst_x >= LCD_WIDTH) continue;
                g_lcd_framebuffer[dst_y * LCD_WIDTH + dst_x] =
                    frame_buf[row * img_w + col];
            }
        }
    }

    heap_caps_free(frame_buf);
    return ESP_OK;
}

/**
 * @brief Draw a single point on LCD
 * @param x     X coordinate
 * @param y     Y coordinate
 * @param color Point color
 */
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT)
        return;

    lcd_set_window(x, y, x, y);

    /* Send big-endian 16-bit pixel data (panel expects high byte first). */
    uint8_t bytes[2] = { (uint8_t)(color >> 8), (uint8_t)(color & 0xFF) };
    gpio_set_level(PIN_NUM_CS, 0);
    gpio_set_level(PIN_NUM_DC, 1);          /* data mode */
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = bytes,
        .flags = 0,
    };
    spi_device_polling_transmit(s_spi, &t);
    gpio_set_level(PIN_NUM_CS, 1);

    /* Update framebuffer for screen capture (big-endian storage). */
    if (g_lcd_framebuffer) {
        g_lcd_framebuffer[y * LCD_WIDTH + x] = (color >> 8) | (color << 8);
    }
}

/**
 * @brief Display 16*16 ASCII and GBK Chinese font on LCD
 * @param x      Start X coordinate of text
 * @param y      Start Y coordinate of text
 * @param fc     Foreground color (RGB565)
 * @param bc     Background color (RGB565)
 * @param s      Pointer to string buffer, support ASCII + GBK encoding
 * @note  Newline character '\n' is supported for line feed
 */
void Gui_DrawFont_GBK16(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t *s)
{
    unsigned char i, j;
    unsigned short k;
    uint16_t x_start = x;  /* Record original X position for line feed */

    if (s == NULL)
        return;

    /* Traverse string until end character '\0' */
    while (*s != 0)
    {
        if (*s < 128)  /* Single-byte ASCII character */
        {
            k = *s;
            if (k == '\n')  /* Handle line feed */
            {
                x = x_start;    /* Reset X coordinate */
                y += 16;        /* Move down one line (16 pixels height) */
                s++;
                continue;
            }

            /* Map space and control characters to font index 0 */
            k = (k > 32) ? (k - 32) : 0;

            /* Draw 16*8 ASCII dot matrix font */
            for (i = 0; i < 16; i++)
            {
                for (j = 0; j < 8; j++)
                {
                    if (asc16[k * 16 + i] & (0x80 >> j))
                        LCD_DrawPoint(x + j, y + i, fc);  /* Draw foreground pixel */
                    else if (fc != bc)
                        LCD_DrawPoint(x + j, y + i, bc);  /* Draw background pixel */
                }
            }
            x += 8;   /* ASCII character occupies 8 pixels width */
            s++;      /* Move pointer to next byte */
        }
        else  /* Double-byte GBK Chinese character */
        {
            /* Match GBK character from font library */
            for (k = 0; k < hz16_num; k++)
            {
                if ((hz16[k].Index[0] == *s) && (hz16[k].Index[1] == *(s + 1)))
                {
                    /* Draw 16*16 Chinese dot matrix font (two 8-pixel width parts) */
                    for (i = 0; i < 16; i++)
                    {
                        /* Left 8 pixels */
                        for (j = 0; j < 8; j++)
                        {
                            if (hz16[k].Msk[i * 2] & (0x80 >> j))
                                LCD_DrawPoint(x + j, y + i, fc);
                            else if (fc != bc)
                                LCD_DrawPoint(x + j, y + i, bc);
                        }
                        /* Right 8 pixels */
                        for (j = 0; j < 8; j++)
                        {
                            if (hz16[k].Msk[i * 2 + 1] & (0x80 >> j))
                                LCD_DrawPoint(x + j + 8, y + i, fc);
                            else if (fc != bc)
                                LCD_DrawPoint(x + j + 8, y + i, bc);
                        }
                    }
                    break;  /* Exit loop once character is matched */
                }
            }
            s += 2;    /* GBK character occupies 2 bytes */
            x += 16;   /* Chinese character occupies 16 pixels width */
        }
    }
}

/**
 * @brief Display 16*16 ASCII and GBK Chinese font on LCD (transparent background)
 *
 * Only draws foreground pixels. Background pixels are left unchanged, so the
 * text appears overlaid on whatever is already on screen. Useful for drawing
 * labels on top of images or thermal data without erasing them.
 *
 * @param[in] x  Start X coordinate of text
 * @param[in] y  Start Y coordinate of text
 * @param[in] fc Foreground color (RGB565)
 * @param[in] s  Pointer to string buffer (ASCII + GBK encoding, '\n' supported)
 */
void lcd_draw_text(int x, int y, uint16_t fc, const uint8_t *s)
{
    if (s == NULL)
        return;

    /* Pass 1: compute bounding box. */
    int max_w = 0, cur_w = 0, lines = 1;
    const uint8_t *p = s;
    while (*p) {
        if (*p == '\n') {
            if (cur_w > max_w) max_w = cur_w;
            cur_w = 0;
            lines++;
            p++;
        } else if (*p < 128) {
            cur_w += 8;
            p++;
        } else {
            cur_w += 16;
            p += 2;
        }
    }
    if (cur_w > max_w) max_w = cur_w;
    int W = max_w;
    int H = lines * 16;
    if (W <= 0 || H <= 0)
        return;

    /* Clamp to screen. */
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + W > LCD_WIDTH)  W = LCD_WIDTH - x;
    if (y + H > LCD_HEIGHT) H = LCD_HEIGHT - y;
    if (W <= 0 || H <= 0)
        return;

    int num = W * H;
    uint16_t *buf = heap_caps_malloc(num * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buf) {
        ESP_LOGE(TAG, "lcd_draw_text: malloc %d failed", num * 2);
        return;
    }

    /* Restore background from framebuffer to preserve transparency.
     * Both framebuffer and buf use big-endian format for SPI, so copy
     * directly without byte swapping. */
    if (g_lcd_framebuffer) {
        for (int row = 0; row < H; row++) {
            int dst_y = y + row;
            if (dst_y < 0 || dst_y >= LCD_HEIGHT) continue;
            for (int col = 0; col < W; col++) {
                int dst_x = x + col;
                if (dst_x < 0 || dst_x >= LCD_WIDTH) continue;
                buf[row * W + col] = g_lcd_framebuffer[dst_y * LCD_WIDTH + dst_x];
            }
        }
    } else {
        memset(buf, 0, num * sizeof(uint16_t));
    }

    /* Pass 2: draw foreground pixels into buffer. */
    int cx = 0, cy = 0;
    unsigned char i, j;
    unsigned short k;
    uint16_t be_fc = (fc >> 8) | (fc << 8);
    p = s;
    while (*p) {
        if (*p == '\n') {
            cx = 0;
            cy += 16;
            p++;
            continue;
        }
        if (*p < 128) {
            k = *p;
            k = (k > 32) ? (k - 32) : 0;
            for (i = 0; i < 16; i++) {
                if (cy + i >= H) break;
                for (j = 0; j < 8; j++) {
                    if (cx + j >= W) break;
                    if (asc16[k * 16 + i] & (0x80 >> j))
                        buf[(cy + i) * W + (cx + j)] = be_fc;
                }
            }
            cx += 8;
            p++;
        } else {
            for (k = 0; k < hz16_num; k++) {
                if (hz16[k].Index[0] == *p && hz16[k].Index[1] == *(p + 1)) {
                    for (i = 0; i < 16; i++) {
                        if (cy + i >= H) break;
                        for (j = 0; j < 8; j++) {
                            if (cx + j >= W) break;
                            if (hz16[k].Msk[i * 2] & (0x80 >> j))
                                buf[(cy + i) * W + (cx + j)] = be_fc;
                        }
                        for (j = 0; j < 8; j++) {
                            if (cx + j + 8 >= W) break;
                            if (hz16[k].Msk[i * 2 + 1] & (0x80 >> j))
                                buf[(cy + i) * W + (cx + j + 8)] = be_fc;
                        }
                    }
                    break;
                }
            }
            cx += 16;
            p += 2;
        }
    }

    /* Transmit the whole text box in one shot. */
    lcd_set_window(x, y, x + W - 1, y + H - 1);
    gpio_set_level(PIN_NUM_CS, 0);
    gpio_set_level(PIN_NUM_DC, 1);
    spi_transaction_t t = {
        .length = num * 16,
        .tx_buffer = buf,
    };
    spi_device_polling_transmit(s_spi, &t);
    gpio_set_level(PIN_NUM_CS, 1);

    /* Update framebuffer: both buf and framebuffer use big-endian. */
    if (g_lcd_framebuffer) {
        for (int row = 0; row < H; row++) {
            int dst_y = y + row;
            if (dst_y < 0 || dst_y >= LCD_HEIGHT) continue;
            for (int col = 0; col < W; col++) {
                int dst_x = x + col;
                if (dst_x < 0 || dst_x >= LCD_WIDTH) continue;
                g_lcd_framebuffer[dst_y * LCD_WIDTH + dst_x] = buf[row * W + col];
            }
        }
    }

    heap_caps_free(buf);
}

/**
 * @brief Display text with a solid background (flicker-free overlay)
 *
 * Renders the whole text box into an off-screen buffer (filled with bg,
 * then foreground pixels overwritten with fc) and transmits it to the
 * panel in a single SPI transaction. Atomic repaint => no flicker.
 */
void lcd_draw_text_bg(int x, int y, uint16_t fc, uint16_t bg, const uint8_t *s)
{
    if (s == NULL)
        return;

    /* Pass 1: compute bounding box (widest line * total height). */
    int max_w = 0, cur_w = 0, lines = 1;
    const uint8_t *p = s;
    while (*p) {
        if (*p == '\n') {
            if (cur_w > max_w) max_w = cur_w;
            cur_w = 0;
            lines++;
            p++;
        } else if (*p < 128) {
            cur_w += 8;
            p++;
        } else {
            cur_w += 16;
            p += 2;
        }
    }
    if (cur_w > max_w) max_w = cur_w;
    int W = max_w;
    int H = lines * 16;
    if (W <= 0 || H <= 0)
        return;

    /* Clamp to screen to avoid writing past the panel edge. */
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + W > LCD_WIDTH)  W = LCD_WIDTH - x;
    if (y + H > LCD_HEIGHT) H = LCD_HEIGHT - y;
    if (W <= 0 || H <= 0)
        return;

    int num = W * H;
    uint16_t *buf = heap_caps_malloc(num * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buf) {
        ESP_LOGE(TAG, "lcd_draw_text_bg: malloc %d failed", num * 2);
        return;
    }

    /* Fill background (big-endian for panel). */
    uint16_t be_bg = (bg >> 8) | (bg << 8);
    uint16_t be_fc = (fc >> 8) | (fc << 8);
    for (int i = 0; i < num; i++)
        buf[i] = be_bg;

    /* Pass 2: draw foreground pixels into buffer. cx/cy are the top-left
     * offset of the current character within the buffer. */
    int cx = 0, cy = 0;
    unsigned char i, j;
    unsigned short k;
    p = s;
    while (*p) {
        if (*p == '\n') {
            cx = 0;
            cy += 16;
            p++;
            continue;
        }
        if (*p < 128) {  /* ASCII */
            k = *p;
            k = (k > 32) ? (k - 32) : 0;
            for (i = 0; i < 16; i++) {
                if (cy + i >= H) break;
                for (j = 0; j < 8; j++) {
                    if (cx + j >= W) break;
                    if (asc16[k * 16 + i] & (0x80 >> j))
                        buf[(cy + i) * W + (cx + j)] = be_fc;
                }
            }
            cx += 8;
            p++;
        } else {  /* GBK */
            for (k = 0; k < hz16_num; k++) {
                if (hz16[k].Index[0] == *p && hz16[k].Index[1] == *(p + 1)) {
                    for (i = 0; i < 16; i++) {
                        if (cy + i >= H) break;
                        for (j = 0; j < 8; j++) {
                            if (cx + j >= W) break;
                            if (hz16[k].Msk[i * 2] & (0x80 >> j))
                                buf[(cy + i) * W + (cx + j)] = be_fc;
                        }
                        for (j = 0; j < 8; j++) {
                            if (cx + j + 8 >= W) break;
                            if (hz16[k].Msk[i * 2 + 1] & (0x80 >> j))
                                buf[(cy + i) * W + (cx + j + 8)] = be_fc;
                        }
                    }
                    break;
                }
            }
            cx += 16;
            p += 2;
        }
    }

    /* Transmit the whole text box in one shot. */
    lcd_set_window(x, y, x + W - 1, y + H - 1);
    gpio_set_level(PIN_NUM_CS, 0);
    gpio_set_level(PIN_NUM_DC, 1);
    spi_transaction_t t = {
        .length = num * 16,
        .tx_buffer = buf,
    };
    spi_device_polling_transmit(s_spi, &t);
    gpio_set_level(PIN_NUM_CS, 1);

    /* Update framebuffer: buf is big-endian, framebuffer is big-endian. */
    if (g_lcd_framebuffer) {
        for (int row = 0; row < H; row++) {
            int dst_y = y + row;
            if (dst_y < 0 || dst_y >= LCD_HEIGHT) continue;
            for (int col = 0; col < W; col++) {
                int dst_x = x + col;
                if (dst_x < 0 || dst_x >= LCD_WIDTH) continue;
                g_lcd_framebuffer[dst_y * LCD_WIDTH + dst_x] = buf[row * W + col];
            }
        }
    }

    heap_caps_free(buf);
}

/**
 * @brief Draw a battery level icon (flicker-free, single SPI transaction)
 *
 * See header comment in lcd_ili9341.h for details.
 */
void lcd_draw_battery(int x, int y, int level)
{
    if (level < 0) level = 0;
    if (level > 100) level = 100;

    const int w = 30, h = 16;
    uint16_t *buf = heap_caps_malloc(w * h * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buf) {
        ESP_LOGE(TAG, "lcd_draw_battery: malloc failed");
        return;
    }

    /* 1) Black background (big-endian 0x0000 = 0). */
    for (int i = 0; i < w * h; i++)
        buf[i] = 0;

    const uint16_t be_white = 0xFFFF;  /* big-endian white */

    /* 2) Battery outline: rect (2,2)-(25,13), 1px border. */
    for (int xi = 2; xi <= 25; xi++) {
        buf[2 * w + xi]  = be_white;   /* top edge */
        buf[13 * w + xi] = be_white;   /* bottom edge */
    }
    for (int yi = 2; yi <= 13; yi++) {
        buf[yi * w + 2]  = be_white;   /* left edge */
        buf[yi * w + 25] = be_white;   /* right edge */
    }

    /* 3) Terminal nub (26,5)-(27,10). */
    for (int yi = 5; yi <= 10; yi++) {
        buf[yi * w + 26] = be_white;
        buf[yi * w + 27] = be_white;
    }

    /* 4) Fill bar (3,3)-(2+fill_w, 12), color by level. */
    int fill_w = (level * 22) / 100;   /* 0..22 px */
    uint16_t be_fill;
    if (level < 30) {
        uint16_t red = 0xF800;
        be_fill = (red >> 8) | (red << 8);
    } else if (level < 60) {
        uint16_t yellow = 0xFFE0;
        be_fill = (yellow >> 8) | (yellow << 8);
    } else {
        uint16_t green = 0x07E0;
        be_fill = (green >> 8) | (green << 8);
    }
    for (int yi = 3; yi <= 12; yi++) {
        for (int xi = 3; xi < 3 + fill_w; xi++) {
            buf[yi * w + xi] = be_fill;
        }
    }

    /* 5) Transmit the whole icon in one shot. */
    lcd_set_window(x, y, x + w - 1, y + h - 1);
    gpio_set_level(PIN_NUM_CS, 0);
    gpio_set_level(PIN_NUM_DC, 1);
    spi_transaction_t t = {
        .length = w * h * 16,
        .tx_buffer = buf,
    };
    spi_device_polling_transmit(s_spi, &t);
    gpio_set_level(PIN_NUM_CS, 1);

    /* Update framebuffer: buf is big-endian, framebuffer is big-endian. */
    if (g_lcd_framebuffer) {
        for (int row = 0; row < h; row++) {
            int dst_y = y + row;
            if (dst_y < 0 || dst_y >= LCD_HEIGHT) continue;
            for (int col = 0; col < w; col++) {
                int dst_x = x + col;
                if (dst_x < 0 || dst_x >= LCD_WIDTH) continue;
                g_lcd_framebuffer[dst_y * LCD_WIDTH + dst_x] = buf[row * w + col];
            }
        }
    }

    heap_caps_free(buf);
}

/**
 * @brief Internal helper that draws an ASCII or GBK string into a caller
 *        supplied buffer of fixed dimensions.
 *
 * Used by lcd_draw_text_fixed so each text segment can be painted into
 * the same backing store before a single SPI transfer.
 *
 * @param[out] buf      Row-major big-endian RGB565 framebuffer, size w*h.
 * @param[in]  w        Framebuffer width in pixels.
 * @param[in]  h        Framebuffer height in pixels.
 * @param[in]  ox       Segment's top-left X within the framebuffer.
 * @param[in]  oy       Segment's top-left Y within the framebuffer.
 * @param[in]  be_fc    Big-endian foreground color to apply.
 * @param[in]  s        Segment string (ASCII + GBK, '\0' terminated).
 */
static void text_draw_into_buf(uint16_t *buf, int w, int h,
                               int ox, int oy, uint16_t be_fc,
                               const uint8_t *s)
{
    if (!buf || !s || w <= 0 || h <= 0)
        return;

    int cx = 0, cy = 0;
    unsigned char i, j;
    unsigned short k;
    const uint8_t *p = s;
    while (*p) {
        if (*p == '\n') {
            cx = 0;
            cy += 16;
            p++;
            continue;
        }
        if (*p < 128) {  /* ASCII */
            k = *p;
            k = (k > 32) ? (k - 32) : 0;
            for (i = 0; i < 16; i++) {
                int py = oy + cy + i;
                if (py >= h) break;
                for (j = 0; j < 8; j++) {
                    int px = ox + cx + j;
                    if (px >= w) break;
                    if (asc16[k * 16 + i] & (0x80 >> j))
                        buf[py * w + px] = be_fc;
                }
            }
            cx += 8;
            p++;
        } else {  /* GBK */
            for (k = 0; k < hz16_num; k++) {
                if (hz16[k].Index[0] == *p && hz16[k].Index[1] == *(p + 1)) {
                    for (i = 0; i < 16; i++) {
                        int py = oy + cy + i;
                        if (py >= h) break;
                        for (j = 0; j < 8; j++) {
                            int px = ox + cx + j;
                            if (px >= w) break;
                            if (hz16[k].Msk[i * 2] & (0x80 >> j))
                                buf[py * w + px] = be_fc;
                        }
                        for (j = 0; j < 8; j++) {
                            int px = ox + cx + j + 8;
                            if (px >= w) break;
                            if (hz16[k].Msk[i * 2 + 1] & (0x80 >> j))
                                buf[py * w + px] = be_fc;
                        }
                    }
                    break;
                }
            }
            cx += 16;
            p += 2;
        }
    }
}

/**
 * @brief Flicker-free text overlay with fixed box size and reusable buffer
 *
 * See header comment in lcd_ili9341.h for usage.
 */
void lcd_draw_text_fixed(int x, int y, int w, int h,
                         uint16_t fc, uint16_t bg, ...)
{
    if (w <= 0 || h <= 0)
        return;

    /* Static reusable DMA buffer. Grows on demand if caller asks for a
     * larger box; never shrinks so repeated calls of the same size hit
     * the fast path without any allocator interaction. */
    static uint16_t *s_buf = NULL;
    static int s_buf_pixels = 0;

    int num = w * h;
    if (num > s_buf_pixels) {
        if (s_buf)
            heap_caps_free(s_buf);
        s_buf = heap_caps_malloc(num * sizeof(uint16_t), MALLOC_CAP_DMA);
        s_buf_pixels = s_buf ? num : 0;
        if (!s_buf) {
            ESP_LOGE(TAG, "lcd_draw_text_fixed: malloc %d failed", num * 2);
            return;
        }
    }

    /* Clamp to screen bounds. */
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    int clipped_w = w;
    int clipped_h = h;
    if (x + clipped_w > LCD_WIDTH)  clipped_w = LCD_WIDTH - x;
    if (y + clipped_h > LCD_HEIGHT) clipped_h = LCD_HEIGHT - y;
    if (clipped_w <= 0 || clipped_h <= 0)
        return;

    /* 1) Fill whole box with background. */
    uint16_t be_bg = (bg >> 8) | (bg << 8);
    uint16_t be_fc = (fc >> 8) | (fc << 8);
    uint16_t *buf = s_buf;
    for (int i = 0; i < num; i++)
        buf[i] = be_bg;

    /* 2) Paint each text segment. Varargs are triplets (int ox, int oy,
     *    const uint8_t *s). A negative ox acts as the sentinel that
     *    terminates the list — we must read ONLY ox before breaking,
     *    otherwise va_arg would consume stack garbage as oy/s and crash. */
    va_list ap;
    va_start(ap, bg);
    while (1) {
        int ox = va_arg(ap, int);
        if (ox < 0)
            break;
        int oy = va_arg(ap, int);
        const uint8_t *s = va_arg(ap, const uint8_t *);
        if (!s)
            break;
        text_draw_into_buf(buf, w, h, ox, oy, be_fc, s);
    }
    va_end(ap);

    /* 3) Clamp transfer rectangle if the box straddles the panel edge. */
    if (clipped_w < w || clipped_h < h) {
        int clipped_num = clipped_w * clipped_h;
        uint16_t *tmp = heap_caps_malloc(clipped_num * sizeof(uint16_t),
                                         MALLOC_CAP_DMA);
        if (!tmp) {
            ESP_LOGE(TAG, "lcd_draw_text_fixed: clip malloc %d failed", clipped_num * 2);
            return;
        }
        for (int y2 = 0; y2 < clipped_h; y2++)
            memcpy(tmp + y2 * clipped_w, buf + y2 * w,
                   clipped_w * sizeof(uint16_t));
        lcd_set_window(x, y, x + clipped_w - 1, y + clipped_h - 1);
        gpio_set_level(PIN_NUM_CS, 0);
        gpio_set_level(PIN_NUM_DC, 1);
        spi_transaction_t t = {
            .length = clipped_num * 16,
            .tx_buffer = tmp,
        };
        spi_device_polling_transmit(s_spi, &t);
        gpio_set_level(PIN_NUM_CS, 1);
        heap_caps_free(tmp);
        return;
    }

    /* 4) Full box on screen — transmit in one shot. */
    lcd_set_window(x, y, x + w - 1, y + h - 1);
    gpio_set_level(PIN_NUM_CS, 0);
    gpio_set_level(PIN_NUM_DC, 1);
    spi_transaction_t t = {
        .length = num * 16,
        .tx_buffer = buf,
    };
    spi_device_polling_transmit(s_spi, &t);
    gpio_set_level(PIN_NUM_CS, 1);
}
