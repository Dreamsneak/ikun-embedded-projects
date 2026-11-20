#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "lcd_ili9341.h"
#include "battery_adc.h"
#include "mlx90640.h"
#include "thermal_display.h"
#include "sd_card.h"
#include "ikun_image.h"

/** @brief Shared battery level (0-100%), updated by battery_adc_task,
 *         read by thermal_imaging_task for the battery icon overlay. */
volatile int g_battery_level = 0;

/** @brief Display freeze flag, toggled by button_task (IO47).
 *         When true, thermal_imaging_task skips rendering so the last
 *         frame stays on screen; battery sampling keeps running. */
volatile bool g_display_paused = false;

/** @brief I2C master SCL pin */
#define I2C_MASTER_SCL_IO 42
/** @brief I2C master SDA pin */
#define I2C_MASTER_SDA_IO 41
/** @brief I2C master clock frequency.
 *         MLX90640 supports up to 1 MHz FastMode+; 1 MHz is required to
 *         fit the ~1.8 KB frame read into the 16 Hz (62.5 ms) budget.
 *         If the wiring/module shows I2C errors at 1 MHz, drop to 400000
 *         (and consider MLX90640_RATE_8_HZ). */
#define I2C_MASTER_FREQ_HZ 1000000
/** @brief I2C master port number */
#define I2C_MASTER_PORT I2C_NUM_0

/**
 * @brief Initialize I2C master interface.
 *
 * Configures the I2C bus with the specified SCL and SDA pins,
 * enables internal pull-up resistors, and sets the clock speed.
 */
static void i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(I2C_MASTER_PORT, &conf);
    i2c_driver_install(I2C_MASTER_PORT, conf.mode, 0, 0, 0);
}

/**
 * @brief LCD display initialization task.
 *
 * Initializes the LCD panel and thermal display module.
 * This task runs continuously but performs no further operations
 * once initialization is complete.
 *
 * @param[in] arg Task argument (unused).
 */
static void lcd_display_task(void *arg)
{
    lcd_init();
    thermal_display_init();

    printf("LCD display task started\n");

    /* Display the ikun image on a white background. */
    fill_screen(0x0000);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    lcd_draw_image2lcd_180(100, 60, gImage_ikun_image, 120, 120);
    printf("Image displayed at (100, 60), size 120x120\n");

    while (1) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Battery ADC sampling task.
 *
 * Initializes the battery ADC driver and periodically samples
 * the battery voltage. Calculates and prints the raw ADC value,
 * ADC input voltage, actual battery voltage, and battery level.
 *
 * @param[in] arg Task argument (unused).
 */
static void battery_adc_task(void *arg)
{
    esp_err_t ret = battery_adc_init();
    if (ret != ESP_OK) {
        printf("Battery ADC init failed: %d\n", ret);
        vTaskDelete(NULL);
        return;
    }

    printf("Battery ADC task started on IO4 (ADC1_CH3)\n");
    printf("Divider: R12=5.1K, R11=10K, Ratio=%.2f\n", BATTERY_DIVIDER_RATIO);

    while (1) {
        uint32_t adc_raw = battery_adc_read_raw();
        uint32_t adc_voltage = battery_adc_read_voltage();
        uint32_t battery_voltage = battery_adc_read_battery_voltage();
        int battery_level = battery_adc_read_level();
        g_battery_level = battery_level;
#if 1
        printf("Battery: RAW=%" PRIu32 ", ADC=%" PRIu32 "mV, VBAT=%" PRIu32 "mV, Level=%d%%\n",
               adc_raw, adc_voltage, battery_voltage, battery_level);
#endif

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Thermal imaging task.
 *
 * Initializes the I2C interface and MLX90640 sensor, then continuously
 * reads thermal frames and renders them on the LCD display.
 *
 * @param[in] arg Task argument (unused).
 */
static void thermal_imaging_task(void *arg)
{
    i2c_master_init();

    mlx90640_config_t config = {
        .i2c_port = I2C_MASTER_PORT,
        .refresh_rate = MLX90640_RATE_16_HZ,
        .emissivity = 0.95f,
    };

    esp_err_t ret = mlx90640_init(&config);
    if (ret != ESP_OK) {
        printf("MLX90640 init failed: %d\n", ret);
        vTaskDelete(NULL);
        return;
    }

    printf("MLX90640 thermal imaging task started\n");

    /* Static: keeps 3 KB off the task stack (mlx90640_init's parameter
     * extraction uses several KB of stack internally). */
    static float temperatures[MLX90640_PIXEL_COUNT];

    /* Pre-fill the text overlay region (80x40, top-left) with black.
     * Thermal rendering skips this rectangle entirely, so it stays black
     * until lcd_draw_text_bg paints the numeric readout on top. */
    fill_rect(0, 0, 80, 40, 0x0000);
    /* Pre-fill the battery icon region (30x20, top-right) with black. */
    fill_rect(290, 0, 320, 20, 0x0000);

    while (1) {
        if (!g_display_paused) {

            ret = mlx90640_read_frame(temperatures);
            if (ret != ESP_OK) {
                /* Frame not available (I2C error / data not ready) — short
                 * retry instead of rendering stale or garbage data. The
                 * blind-mode reader self-paces at the frame period, so this
                 * only affects the rare error path. */
                vTaskDelay(50 / portTICK_PERIOD_MS);
                continue;
            }

            float min_temp, max_temp;
            mlx90640_get_temp_range(temperatures, &min_temp, &max_temp);

            thermal_display_render(temperatures);

            /*
             * Text overlay: thermal image now starts at y=50 (see
             * THERMAL_DISPLAY_Y_OFFSET), so the top 50px band is never touched
             * by thermal rendering. lcd_draw_text_bg repaints each text box
             * atomically (off-screen buffer + single SPI transaction), so the
             * numeric readout updates without any flicker. "%6.1f" keeps the
             * numeric field width constant so the box size never varies.
             */
            char buf[20];
            snprintf(buf, sizeof(buf), "MIN:%6.1f", (double)min_temp);
            lcd_draw_text_bg(0, 2, 0xFFFF, 0x0000, (const uint8_t *)buf);
            snprintf(buf, sizeof(buf), "MAX:%6.1f", (double)max_temp);
            lcd_draw_text_bg(0, 20, 0xFFFF, 0x0000, (const uint8_t *)buf);

            /* Battery icon (top-right), flicker-free single SPI transaction. */
            lcd_draw_battery(290, 0, g_battery_level);

            /* Core temperature marker: red cross at thermal center (160,120)
             * — with THERMAL_FLIP_VERTICAL the screen center maps to source
             * pixel (16,11) of the 32x24 array — plus a numeric label drawn
             * transparently (no background box). The thermal blocks
             * underneath are force-refreshed every frame by the renderer,
             * which erases the previous digits before they are redrawn. */
            float core_temp = temperatures[11 * MLX90640_WIDTH + 16];
            fill_rect(157, 120, 163, 121, 0xF800);  /* horizontal tick */
            fill_rect(160, 117, 161, 123, 0xF800);  /* vertical tick */
            char core_buf[16];
            snprintf(core_buf, sizeof(core_buf), "%.1f\x7F", (double)core_temp);
            lcd_draw_text(165, 113, 0xFFFF, (const uint8_t *)core_buf);
        }

        /* Running: yield only briefly — read_frame() blocks until the next
         * sensor frame is ready (blind mode self-paces at the 62 ms frame
         * period), so this does not spin the CPU. Paused: poll the resume
         * flag at 50 ms. */
        vTaskDelay((g_display_paused ? 50 : 5) / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Button task: toggle display freeze on IO47, capture on IO48.
 *
 * Fully non-blocking per-button state machine. Each 10 ms polling tick
 * drives both buttons independently — no `while (gpio==0)` busy-waits,
 * so a long operation (SD mount / BMP save) on IO48 can never starve
 * the IO47 state machine and vice versa.
 *
 * State machine per button:
 *   BTN_IDLE        : pin == 1 (released). Falling edge → BTN_DEBOUNCE.
 *   BTN_DEBOUNCE    : pin must stay == 0 for DEBOUNCE_TICKS (20 ms).
 *                     If pin bounces back to 1 → return to IDLE.
 *                     Once debounced → emit one ACTION event, then go to
 *                     BTN_WAIT_RELEASE so a single press does not retrigger.
 *   BTN_WAIT_RELEASE: pin must stay == 1 for RELEASE_TICKS (30 ms) to
 *                     confirm release, then go back to BTN_IDLE and accept
 *                     the next press.
 */
typedef enum {
    BTN_IDLE = 0,
    BTN_DEBOUNCE,
    BTN_WAIT_RELEASE,
} btn_state_t;

#define DEBOUNCE_TICKS    2   /* 2 ticks * 10 ms = 20 ms  */
#define RELEASE_TICKS     3   /* 3 ticks * 10 ms = 30 ms  */

static void button_task(void *arg)
{
    const gpio_num_t freeze_io  = 47;  /* Display pause toggle */
    const gpio_num_t capture_io = 48;  /* Save framebuffer to SD */

    /* Configure both buttons as inputs with internal pull-ups.
     * Active-low: external switch shorts pin to GND when pressed. */
    uint64_t pin_mask = (1ULL << freeze_io) | (1ULL << capture_io);
    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    btn_state_t freeze_st  = BTN_IDLE;
    btn_state_t capture_st = BTN_IDLE;
    int         freeze_tick_cnt  = 0;
    int         capture_tick_cnt = 0;

    /* Print startup pin levels so users can verify wiring via the monitor.
     * Expected: both == 1 if button is open (pull-ups active). If either
     * prints 0 at idle the switch is stuck closed or the pull-up is not
     * driving the pin. */
    printf("[BTN] IO47 idle level=%d, IO48 idle level=%d\n",
           gpio_get_level(freeze_io), gpio_get_level(capture_io));

    while (1) {
        /* ------------------ Sampling ------------------ */
        int f = gpio_get_level(freeze_io);
        int c = gpio_get_level(capture_io);

        /* =============================================================
         * IO47: freeze / resume display (lightweight, always fast)
         * ============================================================= */
        switch (freeze_st) {
        case BTN_IDLE:
            if (f == 0) {                          /* falling edge */
                freeze_st = BTN_DEBOUNCE;
                freeze_tick_cnt = 0;
            }
            break;
        case BTN_DEBOUNCE:
            if (f == 0) {
                freeze_tick_cnt++;
                if (freeze_tick_cnt >= DEBOUNCE_TICKS) {
                    /* ---- Stable press confirmed ---- */
                    g_display_paused = !g_display_paused;
                    printf("[BTN] IO47 → Display %s\n",
                           g_display_paused ? "PAUSED" : "RUNNING");
                    freeze_st = BTN_WAIT_RELEASE;
                    freeze_tick_cnt = 0;
                }
            } else {
                /* Bounce back high — ignore the glitch. */
                freeze_st = BTN_IDLE;
            }
            break;
        case BTN_WAIT_RELEASE:
            if (f == 1) {
                freeze_tick_cnt++;
                if (freeze_tick_cnt >= RELEASE_TICKS) {
                    freeze_st = BTN_IDLE;   /* accept next press */
                }
            } else {
                freeze_tick_cnt = 0;     /* still pressed, reset counter */
            }
            break;
        } /* switch freeze */

        /* =============================================================
         * IO48: save framebuffer to SD (may be slow — but because the
         * state machine is polled and the ACTION runs only once, the
         * heavy operation blocks only ONE iteration of this loop; on
         * the very next tick the IO47 state machine is polled again,
         * so even a multi-second SD mount can never make IO47 "dead".
         * ============================================================= */
        switch (capture_st) {
        case BTN_IDLE:
            if (c == 0) {                          /* falling edge */
                capture_st = BTN_DEBOUNCE;
                capture_tick_cnt = 0;
            }
            break;
        case BTN_DEBOUNCE:
            if (c == 0) {
                capture_tick_cnt++;
                if (capture_tick_cnt >= DEBOUNCE_TICKS) {
                    /* ---- Stable press confirmed: perform the action ---- */
                    printf("[BTN] IO48 → Saving frame...\n");
                    if (!sd_card_is_mounted()) {
                        printf("[BTN] First capture — mounting SD card...\n");
                        esp_err_t ret = sd_card_init();
                        if (ret != ESP_OK) {
                            ESP_LOGE("BTN", "SD mount FAIL: %s",
                                     esp_err_to_name(ret));
                            capture_st = BTN_WAIT_RELEASE;
                            capture_tick_cnt = 0;
                            break;   /* don't retry save this press */
                        }
                    }
                    esp_err_t ret = sd_card_save_framebuffer_bmp();
                    if (ret == ESP_OK) {
                        printf("[BTN] IO48 → Save OK\n");
                    } else {
                        ESP_LOGE("BTN", "Save FAIL: %s",
                                 esp_err_to_name(ret));
                    }
                    capture_st = BTN_WAIT_RELEASE;
                    capture_tick_cnt = 0;
                }
            } else {
                capture_st = BTN_IDLE;                /* glitch */
            }
            break;
        case BTN_WAIT_RELEASE:
            if (c == 1) {
                capture_tick_cnt++;
                if (capture_tick_cnt >= RELEASE_TICKS) {
                    capture_st = BTN_IDLE;  /* accept next press */
                }
            } else {
                capture_tick_cnt = 0;
            }
            break;
        } /* switch capture */

        /* Fixed-rate tick. State machines rely on the 10 ms cadence. */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief Application main entry point.
 *
 * Initializes the system and creates four FreeRTOS tasks:
 * 1. LCD display task (core 0)
 * 2. Battery ADC task (core 0)
 * 3. Thermal imaging task (core 1)
 * 4. Button task (core 0) — toggles display freeze on IO47, captures
 *    current framebuffer to SD card on IO48.
 *
 * The SD card is NOT mounted at startup; it is lazily initialised on
 * the first IO48 press so the device can boot without an SD card.
 */
void app_main(void)
{
    printf("ikun_thermal_imaging!\n");
    printf("author: Sneaker\n");

    xTaskCreatePinnedToCore(lcd_display_task, "lcd_display_task", 4096, NULL, 5, NULL, 0);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    xTaskCreatePinnedToCore(battery_adc_task, "battery_adc_task", 4096, NULL, 5, NULL, 0);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    xTaskCreatePinnedToCore(thermal_imaging_task, "thermal_imaging_task", 8192, NULL, 5, NULL, 1);
    /* button_task calls sd_card_init + sd_card_save_framebuffer_bmp (heavy
     * stack use): SD mount, BMP header build, 320x240 pixel conversion loop,
     * fwrite()s. Need plenty of stack to avoid overflow (was 2048 → crashed). */
    xTaskCreatePinnedToCore(button_task, "button_task", 6144, NULL, 4, NULL, 0);

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
