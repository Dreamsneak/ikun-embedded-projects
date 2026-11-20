#include "mlx90640.h"
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "mlx90640";

/**
 * @brief Typical ambient-temperature shift used for the passive-radiation
 *        compensation term (Ta - TaR) in MLX90640_CalculateTo().
 *
 * This is the default from the official Melexis examples (sensor die is
 * ~8 C above ambient due to self-heating). Fine-tune by pointing the
 * sensor at a known-temperature target and comparing the reading.
 */
#define MLX90640_TA_SHIFT 8.0f

/**
 * @brief Words transferred per I2C read transaction.
 *
 * 64 words = 128 bytes keeps every transaction well inside the legacy
 * I2C driver's FIFO-managed limits. The sensor auto-increments its
 * internal address pointer, so chunking is transparent.
 */
#define MLX90640_I2C_CHUNK_WORDS 64

/** @brief I2C port used by the MLX90640 (set during init). */
static i2c_port_t s_i2c_port = I2C_NUM_0;
/** @brief Scene emissivity (set during init, used by CalculateTo). */
static float s_emissivity = 0.95f;
/** @brief Official Melexis calibration parameters (~5 KB, static). */
static paramsMLX90640 s_params;

/** @brief Frame buffer shared by normal and blind read paths (834 words:
 *        [0..767] pixels, [768..831] aux, [832] control, [833] sub-page). */
static uint16_t s_frame[834];

/** @brief True when the sensor's data-ready bit never sets and frames are
 *         read by time-based polling instead (observed on some modules). */
static bool s_blind_mode = false;

/** @brief Alternating sub-page number used for blind-mode frames (status
 *        bit0 reads 0 permanently on affected modules). */
static uint8_t s_blind_subpage = 0;

/** @brief Consecutive GetFrameData timeout counter before blind mode. */
static int s_consec_timeouts = 0;

/** @brief Sub-page period in milliseconds for the configured refresh rate,
 *         used to pace blind-mode reads. Datasheet coding sel (bits [9:7])
 *         gives period = 4000 / 2^sel ms: sel5 (8 Hz) -> 125 ms. */
static uint32_t s_frame_period_ms = 500;

/* ======================================================================
 * Melexis I2C driver contract (MLX90640_I2C_Driver.h) implementation.
 *
 * The critical fix vs. the previous custom driver: the MLX90640 memory
 * address is 16-bit and must be sent as TWO bytes (MSB, LSB) before any
 * read/write. Sending only the low byte (old code) truncated 0x2400/0x0400
 * to 0x00 and made every read return reserved-register garbage (0xFFFF),
 * which produced the constant -271.8 C readings.
 * ====================================================================== */

/**
 * @brief Initialize the I2C interface (no-op; bus configured in main).
 */
void MLX90640_I2CInit(void)
{
    /* Bus is initialized by i2c_master_init() in main.c before this
     * driver is used; nothing to do here. */
}

/**
 * @brief Set I2C bus frequency (no-op; fixed at 400 kHz in main).
 *
 * @param[in] freq Requested frequency in Hz (unused).
 */
void MLX90640_I2CFreqSet(int freq)
{
    (void)freq;
}

/**
 * @brief Issue a general-call reset (address 0x00, command 0x06).
 *
 * @return int 0 on success, -1 on I2C failure.
 */
int MLX90640_I2CGeneralReset(void)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x00 << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x06, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(s_i2c_port, cmd, pdMS_TO_TICKS(200));
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK) ? 0 : -1;
}

/**
 * @brief Write one 16-bit register.
 *
 * Transaction: START, addr+W, regMSB, regLSB, dataMSB, dataLSB, STOP.
 *
 * @param[in] slaveAddr 7-bit slave address.
 * @param[in] writeAddress 16-bit register address.
 * @param[in] data 16-bit value to write.
 * @return int 0 on success, -MLX90640_I2C_NACK_ERROR on failure.
 */
int MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data)
{
    uint8_t buf[4] = {
        (uint8_t)(writeAddress >> 8),   /* register address MSB */
        (uint8_t)(writeAddress & 0xFF), /* register address LSB */
        (uint8_t)(data >> 8),           /* data MSB */
        (uint8_t)(data & 0xFF),         /* data LSB */
    };

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (slaveAddr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, buf, sizeof(buf), true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(s_i2c_port, cmd, pdMS_TO_TICKS(200));
    i2c_cmd_link_delete(cmd);

    return (ret == ESP_OK) ? 0 : -MLX90640_I2C_NACK_ERROR;
}

/**
 * @brief Read nMemAddressRead 16-bit words starting at startAddress.
 *
 * Reads are chunked (MLX90640_I2C_CHUNK_WORDS words per transaction) for
 * compatibility with the legacy I2C driver. Bytes arrive big-endian per
 * register and are reassembled into uint16_t words.
 *
 * @param[in] slaveAddr 7-bit slave address.
 * @param[in] startAddress 16-bit starting register address.
 * @param[in] nMemAddressRead Number of 16-bit words to read.
 * @param[out] data Destination word buffer.
 * @return int 0 on success, -1 on I2C failure.
 */
int MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress,
                     uint16_t nMemAddressRead, uint16_t *data)
{
    uint16_t addr = startAddress;
    uint16_t remaining = nMemAddressRead;
    uint16_t *dst = data;

    while (remaining > 0) {
        uint16_t chunk = (remaining > MLX90640_I2C_CHUNK_WORDS)
                             ? MLX90640_I2C_CHUNK_WORDS
                             : remaining;
        uint8_t buf[MLX90640_I2C_CHUNK_WORDS * 2];

        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (slaveAddr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, (uint8_t)(addr >> 8), true);   /* MSB */
        i2c_master_write_byte(cmd, (uint8_t)(addr & 0xFF), true); /* LSB */
        i2c_master_start(cmd);                                    /* repeated start */
        i2c_master_write_byte(cmd, (slaveAddr << 1) | I2C_MASTER_READ, true);
        if (chunk > 1) {
            i2c_master_read(cmd, buf, chunk * 2 - 1, I2C_MASTER_ACK);
            i2c_master_read_byte(cmd, buf + chunk * 2 - 1, I2C_MASTER_NACK);
        } else {
            /* Single-word read: both bytes go into buf[0..1]. Reading into
             * buf[1] would leave buf[0] as stack garbage (this off-by-one
             * made every 1-word register read return random values). */
            i2c_master_read_byte(cmd, buf, I2C_MASTER_ACK);
            i2c_master_read_byte(cmd, buf + 1, I2C_MASTER_NACK);
        }
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(s_i2c_port, cmd, pdMS_TO_TICKS(500));
        i2c_cmd_link_delete(cmd);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2C read fail @0x%04X (%u words): %s",
                     addr, chunk, esp_err_to_name(ret));
            return -1;
        }

        for (int i = 0; i < chunk; i++) {
            dst[i] = ((uint16_t)buf[2 * i] << 8) | buf[2 * i + 1];
        }

        addr += chunk;
        dst += chunk;
        remaining -= chunk;
    }

    return 0;
}

/* ======================================================================
 * Public driver API (mlx90640.h)
 * ====================================================================== */

/**
 * @brief Initialize the MLX90640 sensor.
 *
 * Dumps EEPROM, extracts calibration with the official algorithm and
 * programs the refresh rate.
 */
esp_err_t mlx90640_init(const mlx90640_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    s_i2c_port = config->i2c_port;
    s_emissivity = config->emissivity;

    /* Precompute the sub-page period from the 3-bit refresh-rate select:
     * sel 0..7 maps to 0.25..32 Hz, i.e. 4000/2^sel milliseconds. */
    s_frame_period_ms = 4000U >> (config->refresh_rate & 0x07);

    MLX90640_I2CInit();

    /* General-call reset (address 0x00, cmd 0x06): forces the sensor to
     * reboot its measurement engine and reload RAM from EEPROM. Recovers
     * the "ACKs on I2C but never measures" state caused by a dirty boot
     * or supply brown-out. */
    MLX90640_I2CGeneralReset();
    vTaskDelay(pdMS_TO_TICKS(200));   /* wait for EEPROM reload (~40 ms typ.) */

    /* 832-word EEPROM dump — buffer is static, ExtractParameters needs
     * several KB of stack for its internal temp arrays. */
    static uint16_t eeData[MLX90640_EEPROM_DUMP_NUM];
    int status = MLX90640_DumpEE(MLX90640_I2C_ADDR, eeData);
    if (status != MLX90640_NO_ERROR) {
        ESP_LOGE(TAG, "EEPROM dump failed: %d", status);
        return ESP_ERR_INVALID_RESPONSE;
    }

    status = MLX90640_ExtractParameters(eeData, &s_params);
    if (status != MLX90640_NO_ERROR) {
        /* Non-zero = broken/outlier pixel count error codes from the API;
         * calibration is still usable for most scenes. */
        ESP_LOGW(TAG, "ExtractParameters warning: %d (calibration loaded)", status);
    }

    status = MLX90640_SetRefreshRate(MLX90640_I2C_ADDR,
                                     config->refresh_rate & 0x07);
    if (status != MLX90640_NO_ERROR) {
        ESP_LOGE(TAG, "SetRefreshRate failed: %d", status);
        return ESP_ERR_INVALID_RESPONSE;
    }

    vTaskDelay(pdMS_TO_TICKS(500));   /* let the first frame complete */

    /* Raw 2-byte status read: prints the exact bytes on the wire to verify
     * byte order independent of any word-assembly logic. */
    uint8_t raw[2] = {0, 0};
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MLX90640_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x80, true);
    i2c_master_write_byte(cmd, 0x00, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MLX90640_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &raw[0], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &raw[1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    if (i2c_master_cmd_begin(s_i2c_port, cmd, pdMS_TO_TICKS(200)) == ESP_OK) {
        ESP_LOGI(TAG, "status raw bytes on wire: %02X %02X", raw[0], raw[1]);
    }
    i2c_cmd_link_delete(cmd);

    /* Poll status + first RAM words: detect whether RAM frame data updates
     * even though the data-ready bit (status bit 3) never sets. */
    bool data_ready_seen = false, ram_changed = false;
    uint16_t ram_prev[3] = {0xFFFF, 0xFFFF, 0xFFFF};
    for (int i = 0; i < 4; i++) {
        uint16_t st = 0, ram[3] = {0};
        (void)MLX90640_I2CRead(MLX90640_I2C_ADDR, 0x8000, 1, &st);
        (void)MLX90640_I2CRead(MLX90640_I2C_ADDR, 0x0400, 3, ram);
        ESP_LOGI(TAG, "poll[%d] status=0x%04X ram=%04X %04X %04X",
                 i, st, ram[0], ram[1], ram[2]);
        if (st & 0x0008) {
            data_ready_seen = true;
        }
        if (memcmp(ram, ram_prev, sizeof(ram)) != 0) {
            ram_changed = true;
        }
        memcpy(ram_prev, ram, sizeof(ram));
        vTaskDelay(pdMS_TO_TICKS(400));
    }

    if (!data_ready_seen && ram_changed) {
        s_blind_mode = true;
        ESP_LOGW(TAG, "RAM updates but dataReady bit never sets -> "
                      "switching to blind frame polling");
    }

    ESP_LOGI(TAG, "MLX90640 initialized (addr=0x%02X, rate_sel=%u, "
                  "emissivity=%.2f)",
             MLX90640_I2C_ADDR, config->refresh_rate & 0x07,
             (double)s_emissivity);
    return ESP_OK;
}

/**
 * @brief Read one frame without waiting for the data-ready bit (blind mode).
 *
 * Mirrors MLX90640_GetFrameData() after its wait loop: clears the status
 * register, reads pixels + aux + control register, then runs the official
 * conversion. Used on modules whose data-ready bit never sets although the
 * RAM frame data keeps updating.
 *
 * @param[out] temperatures Temperature output array (768 elements, Celsius).
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_RESPONSE on I2C or
 *         frame-content errors.
 */
static esp_err_t blind_read_frame(float *temperatures)
{
    /* Pace reads at the configured sub-page period (8 Hz -> 125 ms) using
     * ABSOLUTE periodic timing: the I2C transfer (~50 ms at 400 kHz for
     * 832 words) plus rendering happens between deadlines, so the effective
     * frame rate is not period + transfer time as with a plain vTaskDelay
     * at the top of the loop. If processing ever overruns the period,
     * vTaskDelayUntil returns immediately and we run back-to-back. */
    static TickType_t s_last_wake = 0;
    TickType_t period_ticks = pdMS_TO_TICKS(s_frame_period_ms);
    if (period_ticks == 0) {
        period_ticks = 1;
    }
    if (s_last_wake == 0) {
        s_last_wake = xTaskGetTickCount();
    }
    vTaskDelayUntil(&s_last_wake, period_ticks);

    uint16_t status_reg = 0;
    if (MLX90640_I2CRead(MLX90640_I2C_ADDR, MLX90640_STATUS_REG, 1,
                         &status_reg) != MLX90640_NO_ERROR) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* Clear status bits the same way GetFrameData does after a ready frame. */
    (void)MLX90640_I2CWrite(MLX90640_I2C_ADDR, MLX90640_STATUS_REG,
                            MLX90640_INIT_STATUS_VALUE);

    if (MLX90640_I2CRead(MLX90640_I2C_ADDR, MLX90640_PIXEL_DATA_START_ADDRESS,
                         MLX90640_PIXEL_NUM, s_frame) != MLX90640_NO_ERROR) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint16_t aux[MLX90640_AUX_NUM];
    if (MLX90640_I2CRead(MLX90640_I2C_ADDR, MLX90640_AUX_DATA_START_ADDRESS,
                         MLX90640_AUX_NUM, aux) != MLX90640_NO_ERROR) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint16_t ctrl = 0;
    int error = MLX90640_I2CRead(MLX90640_I2C_ADDR, MLX90640_CTRL_REG, 1, &ctrl);
    s_frame[832] = ctrl;
    /* Status bit0 (sub-page) reads 0 permanently on affected modules, so
     * alternate the sub-page manually to refresh both chess halves. */
    s_frame[833] = s_blind_subpage;
    s_blind_subpage ^= 1;
    if (error != MLX90640_NO_ERROR) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* Light-weight validation replacing the static helpers inside the
     * library: reject frames containing the 0x7FFF invalid-data pattern. */
    bool aux_ok = true, pix_ok = true;
    for (int i = 0; i < MLX90640_AUX_NUM; i++) {
        if (aux[i] == 0x7FFF) {
            aux_ok = false;
            break;
        }
    }
    for (int i = 0; i < MLX90640_PIXEL_NUM; i++) {
        if (s_frame[i] == 0x7FFF) {
            pix_ok = false;
            break;
        }
    }
    if (!pix_ok) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (aux_ok) {
        memcpy(&s_frame[MLX90640_PIXEL_NUM], aux,
               MLX90640_AUX_NUM * sizeof(uint16_t));
    }

    float ta = MLX90640_GetTa(s_frame, &s_params);
    float tr = ta - MLX90640_TA_SHIFT;
    MLX90640_CalculateTo(s_frame, &s_params, s_emissivity, tr, temperatures);

    return ESP_OK;
}

/**
 * @brief Read one frame and convert to Celsius with the official algorithm.
 */
esp_err_t mlx90640_read_frame(float *temperatures)
{
    if (!temperatures) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_blind_mode) {
        return blind_read_frame(temperatures);
    }

    int status = MLX90640_GetFrameData(MLX90640_I2C_ADDR, s_frame);
    if (status < 0) {
        /* Negative = I2C/frame-validation error; 0/1 = sub-page number. */
        ESP_LOGW(TAG, "GetFrameData error: %d", status);
        if (++s_consec_timeouts >= 3) {
            s_blind_mode = true;
            ESP_LOGW(TAG, "dataReady never set after repeated timeouts -> "
                          "switching to blind frame polling");
        }
        return ESP_ERR_INVALID_RESPONSE;
    }
    s_consec_timeouts = 0;

    float ta = MLX90640_GetTa(s_frame, &s_params);
    float tr = ta - MLX90640_TA_SHIFT;
    MLX90640_CalculateTo(s_frame, &s_params, s_emissivity, tr, temperatures);

    return ESP_OK;
}

/**
 * @brief Get the minimum and maximum temperatures from a frame.
 */
void mlx90640_get_temp_range(const float *temperatures, float *min_temp, float *max_temp)
{
    if (!temperatures || !min_temp || !max_temp) {
        return;
    }

    *min_temp = temperatures[0];
    *max_temp = temperatures[0];

    for (int i = 1; i < MLX90640_PIXEL_COUNT; i++) {
        if (temperatures[i] < *min_temp) {
            *min_temp = temperatures[i];
        }
        if (temperatures[i] > *max_temp) {
            *max_temp = temperatures[i];
        }
    }
}

/**
 * @brief Convert temperature to RGB565 color using an ironbow palette.
 *
 * Ironbow (black -> deep purple -> magenta -> red -> orange -> yellow ->
 * white) is perceptually monotonic in brightness: hotter areas are always
 * brighter, which makes object boundaries far easier to distinguish than a
 * rainbow gradient. Colors are piecewise-linearly interpolated between
 * fixed control points.
 *
 * @param[in] temp     Temperature to map.
 * @param[in] min_temp Color-range low bound (values below clamp to the first stop).
 * @param[in] max_temp Color-range high bound (values above clamp to the last stop).
 * @return uint16_t RGB565 color (native endianness; caller byte-swaps for SPI).
 */
uint16_t mlx90640_temp_to_color(float temp, float min_temp, float max_temp)
{
    if (max_temp <= min_temp) {
        return 0x0000;
    }

    float normalized = (temp - min_temp) / (max_temp - min_temp);
    if (normalized < 0.0f) {
        normalized = 0.0f;
    } else if (normalized > 1.0f) {
        normalized = 1.0f;
    }

    /* Smoothstep S-curve: steepens the mapping around mid-grey (where most
     * scene content lies) and flattens it near the clipped extremes, which
     * are already saturated to the palette endpoints anyway. Boosts
     * midtone contrast so same-temperature regions separate more clearly. */
    normalized = normalized * normalized * (3.0f - 2.0f * normalized);

    static const struct {
        float t;      /**< Position along the normalized range. */
        uint8_t r, g, b;
    } stops[] = {
        {0.00f,   0,   0,   0},
        {0.15f,  32,   0,  80},
        {0.30f, 120,   0, 120},
        {0.45f, 200,  30,  60},
        {0.60f, 240, 100,  20},
        {0.75f, 255, 180,  30},
        {0.90f, 255, 230, 120},
        {1.00f, 255, 255, 255},
    };
    const int n_stops = sizeof(stops) / sizeof(stops[0]);

    /* Locate the segment containing the normalized value. */
    int seg = 0;
    while (seg < n_stops - 2 && normalized > stops[seg + 1].t) {
        seg++;
    }

    float span = stops[seg + 1].t - stops[seg].t;
    float f = (span > 0.0f) ? (normalized - stops[seg].t) / span : 0.0f;

    uint8_t r = (uint8_t)(stops[seg].r + f * (stops[seg + 1].r - stops[seg].r));
    uint8_t g = (uint8_t)(stops[seg].g + f * (stops[seg + 1].g - stops[seg].g));
    uint8_t b = (uint8_t)(stops[seg].b + f * (stops[seg + 1].b - stops[seg].b));

    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}
