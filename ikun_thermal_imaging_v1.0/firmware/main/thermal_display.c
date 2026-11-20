#include "thermal_display.h"
#include <string.h>
#include <stdlib.h>
#include "lcd_ili9341.h"
#include "mlx90640.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

/**
 * @brief qsort comparator for ascending float order.
 */
static int cmp_float(const void *a, const void *b)
{
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}

/** @brief Percentile bounds for the color-mapping range. Tighter clipping
 *         (1%/99%) yields stronger contrast than raw min/max by preventing a
 *         handful of outlier pixels from compressing the palette. */
#define COLOR_CLIP_LO 0.01f
#define COLOR_CLIP_HI 0.99f

/** @brief Per-pixel temporal IIR factor. At 16 Hz the raw MLX90640 output is
 *         noticeably noisy; each displayed pixel is filtered as
 *         f += TEMP_IIR_ALPHA * (t - f), trading a small motion lag
 *         (~1/alpha frames) for a stable image whose colors only change
 *         when the scene really changes. */
#define TEMP_IIR_ALPHA 0.25f

/** @brief Force a full repaint once every N frames (~3 s at 16 Hz) so slow
 *         drift that never crosses a palette step still becomes visible. */
#define FORCE_FULL_EVERY 48

/** @brief Thermal block rows that always get re-sent. They underlay the
 *         center crosshair (y 117-123) and its TRANSPARENT temperature
 *         label (y 113-129) drawn by main.c: transparent text cannot erase
 *         its previous glyphs, so the thermal background underneath must
 *         be repainted first. Re-sending identical GRAM data is invisible,
 *         so this causes no flicker in static scenes. */
#define FORCE_DIRTY_BLOCK_LO 11
#define FORCE_DIRTY_BLOCK_HI 12

/** @brief Flip the thermal image vertically (module mounted upside down
 *         relative to the panel: screen block ty samples source row
 *         HEIGHT-1-ty). Set to 0 to restore native sensor row order. */
#define THERMAL_FLIP_VERTICAL 1

/** @brief IIR-filtered temperature of every source pixel. */
static float s_filt[MLX90640_PIXEL_COUNT];
/** @brief False until the first frame seeds s_filt. */
static bool s_filt_valid = false;
/** @brief Mapped color of every source pixel in the previous frame, used for
 *         block-level change detection. */
static uint16_t s_prev_color[MLX90640_PIXEL_COUNT];
/** @brief False until the first frame populates s_prev_color (forces a full
 *         repaint on the first frame). */
static bool s_prev_valid = false;
/** @brief Frame counter driving the periodic full repaint. */
static uint16_t s_frame_cnt = 0;

/** @brief Frame buffer for screen capture.
 *  Stores RGB565 pixels in big-endian order (matching SPI transmission order).
 *  Size: LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t) = 153600 bytes. */
uint16_t *g_lcd_framebuffer = NULL;

/** @brief Flag indicating whether display module has been initialized */
static bool s_display_initialized = false;

/**
 * @brief Update a rectangular region in the screen capture framebuffer.
 *
 * Copies pixels from @p data into g_lcd_framebuffer. The data is in
 * native (little-endian) RGB565 format; we byte-swap to big-endian
 * so the framebuffer matches what the SPI panel received.
 *
 * @param[in] x1 Start X coordinate.
 * @param[in] y1 Start Y coordinate.
 * @param[in] x2 End X coordinate (exclusive).
 * @param[in] y2 End Y coordinate (exclusive).
 * @param[in] data Source pixel buffer (native RGB565).
 */
void framebuf_update_rect(int x1, int y1, int x2, int y2, const uint16_t *data)
{
    if (!g_lcd_framebuffer || !data) return;

    int width = x2 - x1;
    int height = y2 - y1;
    if (width <= 0 || height <= 0) return;

    for (int y = 0; y < height; y++) {
        int dst_y = y1 + y;
        if (dst_y < 0 || dst_y >= LCD_HEIGHT) continue;
        for (int x = 0; x < width; x++) {
            int dst_x = x1 + x;
            if (dst_x < 0 || dst_x >= LCD_WIDTH) continue;
            uint16_t native = data[y * width + x];
            g_lcd_framebuffer[dst_y * LCD_WIDTH + dst_x] =
                (native >> 8) | (native << 8);
        }
    }
}

/**
 * @brief Initialize the thermal display module.
 *
 * Sets the initialization flag to true. Must be called after LCD initialization.
 *
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t thermal_display_init(void)
{
    /* Allocate the frame buffer for screen capture (RGB565 big-endian).
     * Size: 320 * 240 * 2 = 153600 bytes.
     * NOTE: This buffer is NOT used for SPI DMA — it is only read by the
     * SD-card save code and written by LCD rendering functions. So we
     * MUST NOT request MALLOC_CAP_DMA (internal DMA RAM is tiny, ~320KB
     * total and contested by SPI/LCD drivers). Prefer SPIRAM first,
     * fall back to any 8-bit addressable RAM. */
    size_t fb_size = (size_t)LCD_WIDTH * (size_t)LCD_HEIGHT * sizeof(uint16_t);

    /* 1) SPIRAM if available (largest, uncontested pool). */
    g_lcd_framebuffer = heap_caps_malloc(fb_size, MALLOC_CAP_SPIRAM);
    /* 2) Default RAM (internal SRAM, 8-bit capable). */
    if (!g_lcd_framebuffer)
        g_lcd_framebuffer = heap_caps_malloc(fb_size, MALLOC_CAP_DEFAULT);
    /* 3) Any 8-bit capable RAM as last resort. */
    if (!g_lcd_framebuffer)
        g_lcd_framebuffer = heap_caps_malloc(fb_size, MALLOC_CAP_8BIT);

    if (!g_lcd_framebuffer) {
        ESP_LOGE("thermal", "framebuffer alloc FAILED (%zu bytes) — "
                 "screen capture will be disabled", fb_size);
        /* Rendering continues to work (non-fatal), save_framebuffer_to_sd()
         * checks this pointer before writing. */
    } else {
        memset(g_lcd_framebuffer, 0, fb_size);
        ESP_LOGI("thermal", "framebuffer OK at %p (%zu bytes)",
                 (void *)g_lcd_framebuffer, fb_size);
    }

    s_display_initialized = true;
    return ESP_OK;
}

/**
 * @brief Display thermal image on LCD with block-level partial refresh.
 *
 * Converts temperature data to RGB565 colors with per-pixel temporal IIR
 * filtering and bilinear 10x upscaling, then transmits only the 10-line
 * blocks whose filtered content changed since the previous frame: static
 * scenes generate no SPI traffic, while moving heat sources update locally.
 * A full repaint is forced on the first frame and once every
 * FORCE_FULL_EVERY frames as a slow-drift safety net.
 *
 * @param[in] temperatures Array of temperature values from MLX90640 (768 elements).
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized,
 *         ESP_ERR_NO_MEM if the block buffer cannot be allocated.
 */
esp_err_t thermal_display_render(const float *temperatures)
{
    if (!s_display_initialized || !temperatures) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Per-pixel temporal IIR low-pass: rejects 16 Hz frame noise so static
     * areas map to identical colors (and get skipped below), while a moving
     * heat source crosses palette steps within a few frames. */
    if (!s_filt_valid) {
        memcpy(s_filt, temperatures, sizeof(s_filt));
        s_filt_valid = true;
    } else {
        for (int i = 0; i < MLX90640_PIXEL_COUNT; i++) {
            s_filt[i] += TEMP_IIR_ALPHA * (temperatures[i] - s_filt[i]);
        }
    }

    /* Percentile-clipped color range, computed from the FILTERED frame:
     * sort a copy and map colors between the 1st and 99th percentile. A few
     * hot/cold outliers would otherwise compress the whole palette onto the
     * remaining pixels. Only the COLOR mapping uses lo/hi — the caller's
     * MIN/MAX text still shows the real extremes. */
    static float s_sorted[MLX90640_PIXEL_COUNT];
    memcpy(s_sorted, s_filt, sizeof(s_sorted));
    qsort(s_sorted, MLX90640_PIXEL_COUNT, sizeof(float), cmp_float);

    float lo = s_sorted[(int)(MLX90640_PIXEL_COUNT * COLOR_CLIP_LO)];
    float hi = s_sorted[(int)(MLX90640_PIXEL_COUNT * COLOR_CLIP_HI)];

    /* Near-isothermal scene: widen around the midpoint so the palette still
     * spans a meaningful range and division stays safe. */
    if (hi - lo < 0.5f) {
        float mid = 0.5f * (lo + hi);
        lo = mid - 0.25f;
        hi = mid + 0.25f;
    }

    /* Partial (block-level) refresh: map each filtered source pixel's color
     * and compare against the previous frame. A bilinear block row depends
     * on source rows ty and ty+1, so a block is re-sent iff either row
     * changed. Static scenes then produce ZERO SPI traffic; moving heat
     * sources update locally. A periodic full repaint catches slow drift. */
    bool force_all = ((++s_frame_cnt % FORCE_FULL_EVERY) == 0) || !s_prev_valid;
    static uint16_t s_cur_color[MLX90640_PIXEL_COUNT];
    bool row_dirty[MLX90640_HEIGHT] = {false};
    for (int ty = 0; ty < MLX90640_HEIGHT; ty++) {
        for (int tx = 0; tx < MLX90640_WIDTH; tx++) {
            int idx = ty * MLX90640_WIDTH + tx;
            s_cur_color[idx] = swap_gb(
                mlx90640_temp_to_color(s_filt[idx], lo, hi));
            if (force_all || s_cur_color[idx] != s_prev_color[idx]) {
                row_dirty[ty] = true;
            }
        }
    }
    memcpy(s_prev_color, s_cur_color, sizeof(s_prev_color));
    s_prev_valid = true;

    /* A block covers one thermal row: width = 32 * zoom, height = zoom lines. */
    int block_width = MLX90640_WIDTH * THERMAL_ZOOM_FACTOR;
    int block_height = THERMAL_ZOOM_FACTOR;

    uint16_t *block = heap_caps_malloc(block_width * block_height * sizeof(uint16_t),
                                       MALLOC_CAP_DMA);
    if (!block) {
        return ESP_ERR_NO_MEM;
    }

    /* Side buffer for blocks that overlap the overlay region.
     * - Rows overlapping the battery (y < BATTERY_OVERLAY_H): transmit only
     *   the middle band [80, 200) — both text box and battery are skipped.
     * - Rows below battery but still in text band: transmit the right part
     *   [80, block_width) — only the text box is skipped.
     * Static for reuse. Buffer sized for the larger (right) segment. */
    static uint16_t *side_buf = NULL;
    static int side_buf_pixels = 0;
    int mid_x0 = TEXT_OVERLAY_X + TEXT_OVERLAY_W;   /* 80 */
    int mid_x1 = BATTERY_OVERLAY_X;                 /* 200 */
    int mid_w = mid_x1 - mid_x0;                    /* 120 */
    int right_w = block_width - TEXT_OVERLAY_W;     /* 240 */
    int side_need = right_w * block_height;          /* max segment width */
    if (side_need > side_buf_pixels) {
        if (side_buf)
            heap_caps_free(side_buf);
        side_buf = heap_caps_malloc(side_need * sizeof(uint16_t), MALLOC_CAP_DMA);
        side_buf_pixels = side_buf ? side_need : 0;
    }

    for (int ty = 0; ty < MLX90640_HEIGHT; ty++) {
        /* Map the screen block to its source rows. With vertical flip the
         * screen top block samples the BOTTOM source row, and bilinear
         * interpolation walks toward source row src_ty-1 going down. */
#if THERMAL_FLIP_VERTICAL
        int src_ty0 = MLX90640_HEIGHT - 1 - ty;
        int src_ty1 = (src_ty0 > 0) ? src_ty0 - 1 : src_ty0;
#else
        int src_ty0 = ty;
        int src_ty1 = (ty < MLX90640_HEIGHT - 1) ? ty + 1 : ty;
#endif

        /* Skip blocks whose bilinear inputs did not change color, except the
         * blocks underlaying the transparent center label/crosshair (must be
         * repainted before main.c redraws the text on top). */
        bool forced = (ty >= FORCE_DIRTY_BLOCK_LO && ty <= FORCE_DIRTY_BLOCK_HI);
        if (!forced && !row_dirty[src_ty0] && !row_dirty[src_ty1]) {
            continue;
        }

        /* Bilinear 10x upscale. For each of the block's ZOOM screen rows:
         *  1) interpolate a 32-element row vertically between the two
         *     mapped source rows (weight row/ZOOM);
         *  2) expand each element horizontally between columns tx and tx+1
         *     (weight i/ZOOM).
         * Border source rows/columns are clamped, so the frame edges stay
         * sharp instead of bleeding outside the array. At weight 0 the
         * sample lands exactly on the source pixel, matching the old
         * nearest-neighbor anchors; intermediate positions now blend. */
        const float *src_row0 = s_filt + src_ty0 * MLX90640_WIDTH;
        const float *src_row1 = s_filt + src_ty1 * MLX90640_WIDTH;
        float vline[MLX90640_WIDTH];

        for (int row = 0; row < block_height; row++) {
            float wy = (float)row / (float)THERMAL_ZOOM_FACTOR;
            for (int tx = 0; tx < MLX90640_WIDTH; tx++) {
                vline[tx] = src_row0[tx] + (src_row1[tx] - src_row0[tx]) * wy;
            }

            uint16_t *dst = block + row * block_width;
            for (int tx = 0; tx < MLX90640_WIDTH; tx++) {
                int x1 = (tx < MLX90640_WIDTH - 1) ? tx + 1 : tx;
                float t0 = vline[tx];
                float dt = vline[x1] - t0;
                for (int i = 0; i < THERMAL_ZOOM_FACTOR; i++) {
                    float wx = (float)i / (float)THERMAL_ZOOM_FACTOR;
                    float t = t0 + dt * wx;
                    /* swap_gb() is a no-op — BGR=1 in MADCTL handles it. */
                    dst[tx * THERMAL_ZOOM_FACTOR + i] =
                        swap_gb(mlx90640_temp_to_color(t, lo, hi));
                }
            }
        }

        /* Send the whole block in a single SPI transaction. */
        int y_start = ty * THERMAL_ZOOM_FACTOR + THERMAL_DISPLAY_Y_OFFSET;

        /* Blocks overlapping the overlay band skip the reserved regions:
         * - y < 20 (battery height): skip both text box [0,80) and battery
         *   [200,240), transmit middle band [80,200).
         * - 20 <= y < 40 (text band only): skip text box, transmit right
         *   part [80, block_width).
         * - y >= 40: transmit whole block. */
        if (y_start < TEXT_OVERLAY_Y + TEXT_OVERLAY_H && side_buf) {
            if (y_start < BATTERY_OVERLAY_Y + BATTERY_OVERLAY_H) {
                for (int row = 0; row < block_height; row++) {
                    memcpy(side_buf + row * mid_w,
                           block + row * block_width + mid_x0,
                           mid_w * sizeof(uint16_t));
                }
                lcd_draw_bitmap(mid_x0, y_start, mid_x1,
                                y_start + block_height, side_buf);
            } else {
                for (int row = 0; row < block_height; row++) {
                    memcpy(side_buf + row * right_w,
                           block + row * block_width + TEXT_OVERLAY_W,
                           right_w * sizeof(uint16_t));
                }
                lcd_draw_bitmap(TEXT_OVERLAY_W, y_start,
                                TEXT_OVERLAY_W + right_w,
                                y_start + block_height, side_buf);
            }
        } else {
            lcd_draw_bitmap(THERMAL_DISPLAY_X_OFFSET, y_start,
                            THERMAL_DISPLAY_X_OFFSET + block_width,
                            y_start + block_height, block);
        }
    }

    heap_caps_free(block);
    return ESP_OK;
}

/**
 * @brief Draw temperature scale on LCD.
 *
 * Draws a vertical color gradient scale on the right side of the screen
 * showing the color mapping from minimum to maximum temperature. The entire
 * scale is built in a single DMA-capable buffer and sent in one SPI transaction.
 *
 * @param[in] min_temp Minimum temperature value.
 * @param[in] max_temp Maximum temperature value.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized,
 *         ESP_ERR_NO_MEM if the scale buffer cannot be allocated.
 */
esp_err_t thermal_display_draw_scale(float min_temp, float max_temp)
{
    if (!s_display_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    int scale_x = MLX90640_WIDTH * THERMAL_ZOOM_FACTOR + 10;
    int scale_y_start = 20;
    int scale_height = MLX90640_HEIGHT * THERMAL_ZOOM_FACTOR;
    int scale_width = 20;

    uint16_t *scale_buf = heap_caps_malloc(scale_width * scale_height * sizeof(uint16_t),
                                           MALLOC_CAP_DMA);
    if (!scale_buf) {
        return ESP_ERR_NO_MEM;
    }

    for (int y = 0; y < scale_height; y++) {
        float normalized = (float)y / scale_height;
        float temp = min_temp + normalized * (max_temp - min_temp);
        uint16_t color = mlx90640_temp_to_color(temp, min_temp, max_temp);
        for (int x = 0; x < scale_width; x++) {
            scale_buf[y * scale_width + x] = color;
        }
    }

    lcd_draw_bitmap(scale_x, scale_y_start,
                    scale_x + scale_width, scale_y_start + scale_height, scale_buf);
    heap_caps_free(scale_buf);

    return ESP_OK;
}
