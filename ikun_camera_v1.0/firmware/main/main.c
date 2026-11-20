#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "lcd_ili9341.h"
#include "ikun_image.h"

static const char *TAG = "ikun_camera";

/**
 * @defgroup camera_pins OV2640 Camera Pin Map (from board schematic)
 * @{
 */
#define CAM_PIN_PWDN    -1  /**< Power-down, not connected */
#define CAM_PIN_RESET   46  /**< OV_RESET */
#define CAM_PIN_XCLK    3   /**< OV_XCLK */
#define CAM_PIN_SIOD    41  /**< OV_SDA (SCCB data) */
#define CAM_PIN_SIOC    42  /**< OV_SCL (SCCB clock) */

#define CAM_PIN_D7      17  /**< OV5640_D7 */
#define CAM_PIN_D6      16  /**< OV5640_D6 */
#define CAM_PIN_D5      15  /**< OV5640_D5 */
#define CAM_PIN_D4      20  /**< OV5640_D4 (unlabeled in schematic, check if wrong) */
#define CAM_PIN_D3      7   /**< OV5640_D3 */
#define CAM_PIN_D2      6   /**< OV5640_D2 */
#define CAM_PIN_D1      5   /**< OV5640_D1 */
#define CAM_PIN_D0      4   /**< OV5640_D0 */

#define CAM_PIN_VSYNC   8   /**< OV_VSYNC */
#define CAM_PIN_HREF    19  /**< OV_HREF */
#define CAM_PIN_PCLK    18  /**< OV_PCLK */
/** @} */

/**
 * @brief Initialize the OV2640 camera sensor.
 *
 * Configures the esp32-camera driver with the board's pin map and captures
 * QVGA (320x240) RGB565 frames, matching the LCD panel resolution. The
 * frame buffer lives in internal DRAM (no PSRAM on this module).
 *
 * @return esp_err_t ESP_OK on success, otherwise the esp_camera_init error.
 */
static esp_err_t camera_init(void)
{
    camera_config_t config = {
        .pin_pwdn     = CAM_PIN_PWDN,
        .pin_reset    = CAM_PIN_RESET,
        .pin_xclk     = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,

        .pin_d7    = CAM_PIN_D7,
        .pin_d6    = CAM_PIN_D6,
        .pin_d5    = CAM_PIN_D5,
        .pin_d4    = CAM_PIN_D4,
        .pin_d3    = CAM_PIN_D3,
        .pin_d2    = CAM_PIN_D2,
        .pin_d1    = CAM_PIN_D1,
        .pin_d0    = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href  = CAM_PIN_HREF,
        .pin_pclk  = CAM_PIN_PCLK,

        .xclk_freq_hz = 20000000,           /* 20 MHz XCLK */
        .ledc_timer   = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_RGB565,   /* direct LCD preview, no decode */
        .frame_size   = FRAMESIZE_QVGA,     /* 320x240, matches the panel */
        .jpeg_quality = 12,                 /* unused for RGB565 */
        .fb_count     = 1,
        .fb_location  = CAMERA_FB_IN_DRAM,  /* module has no PSRAM */
        .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: 0x%x", err);
    }
    return err;
}

void app_main(void)
{
    /* ikun_camera */
    printf("ikun_camera!\n");
    printf("author: Sneaker\n");

    /* Initialize the LCD and show the ikun image centered on screen */
    lcd_init();
    fill_screen(0x0000); /* black background */

    /* gImage_ikun_image is 120x120 (from its 8-byte Image2Lcd header):
     * center on the 320x240 panel at (100, 60) */
    esp_err_t err = lcd_draw_image2lcd_180(100, 60, gImage_ikun_image, 120, 120);
    printf("Draw ikun image: %s\n", esp_err_to_name(err));

    /* Keep the boot logo visible for a moment before starting preview */
    vTaskDelay(pdMS_TO_TICKS(1500));

    /* Initialize the camera; keep retrying so a loose FPC is recoverable */
    while (camera_init() != ESP_OK) {
        ESP_LOGW(TAG, "Retrying camera init...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "Camera ready, starting preview");

    /* Preview loop: capture a QVGA RGB565 frame and push it to the LCD.
     * The camera framebuffer is big-endian RGB565 in memory, which is the
     * exact byte order the ILI9341 expects on the wire — zero-copy send. */
    bool fb_info_logged = false;
    while (1) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            if (!fb_info_logged) {
                ESP_LOGI(TAG, "fb %ux%u len=%u", fb->width, fb->height,
                         (unsigned)fb->len);
                fb_info_logged = true;
            }
            lcd_write_frame(0, 0, LCD_WIDTH, LCD_HEIGHT, fb->buf, fb->len);
            esp_camera_fb_return(fb);
        } else {
            ESP_LOGW(TAG, "Frame capture failed");
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}
