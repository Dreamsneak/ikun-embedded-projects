/**
 * @copyright (C) 2017 Melexis N.V.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifndef _MLX90640_API_H_
#define _MLX90640_API_H_

/**
 * @defgroup mlx90640_api MLX90640 API
 * @brief Official Melexis driver API for the MLX90640 32x24 IR thermal array.
 *
 * Provides EEPROM calibration parameter extraction, frame acquisition and
 * object-temperature (To) calculation. All I2C transfers are delegated to
 * the platform-specific MLX90640_I2C_Driver.
 *
 * @{
 */

/** @brief Function return codes. Negative values are returned for errors
 *         (e.g. -MLX90640_FRAME_DATA_ERROR) so that non-negative results
 *         such as the sub-page number remain valid. */
#define MLX90640_NO_ERROR 0
#define MLX90640_I2C_NACK_ERROR 1
#define MLX90640_I2C_WRITE_ERROR 2
#define MLX90640_BROKEN_PIXELS_NUM_ERROR 3
#define MLX90640_OUTLIER_PIXELS_NUM_ERROR 4
#define MLX90640_BAD_PIXELS_NUM_ERROR 5
#define MLX90640_ADJACENT_BAD_PIXELS_ERROR 6
#define MLX90640_EEPROM_DATA_ERROR 7
#define MLX90640_FRAME_DATA_ERROR 8
#define MLX90640_MEAS_TRIGGER_ERROR 9

/** @brief Single-bit mask for bit position @p x. */
#define BIT_MASK(x) (1UL << (x))
/** @brief Mask covering @p nbits bits starting at @p sbit (bits outside the
 *         field are set, so use ~REG_MASK() to isolate the field). */
#define REG_MASK(sbit,nbits) ~((~(~0UL << (nbits))) << (sbit))

/** @name Device memory map
 *  MLX90640 register/RAM addresses are 16-bit (MSB sent first on I2C).
 *  @{ */
#define MLX90640_EEPROM_START_ADDRESS 0x2400
/** @brief Number of 16-bit words in EEPROM (calibration data). */
#define MLX90640_EEPROM_DUMP_NUM 832
#define MLX90640_PIXEL_DATA_START_ADDRESS 0x0400
/** @brief Total pixel count: 32 columns x 24 rows. */
#define MLX90640_PIXEL_NUM 768
/** @brief Number of rows in the sensor array. */
#define MLX90640_LINE_NUM 24
/** @brief Number of columns in the sensor array. */
#define MLX90640_COLUMN_NUM 32
/** @brief Alias for COLUMN_NUM (used in some legacy naming). */
#define MLX90640_LINE_SIZE 32
/** @brief Alias for LINE_NUM (used in some legacy naming). */
#define MLX90640_COLUMN_SIZE 24
#define MLX90640_AUX_DATA_START_ADDRESS 0x0700
/** @brief Number of auxiliary data words (Vdd, PTAT, etc.) read from RAM. */
#define MLX90640_AUX_NUM 64
#define MLX90640_STATUS_REG 0x8000
/** @brief Value written to STATUS_REG to clear the data-ready flag. */
#define MLX90640_INIT_STATUS_VALUE 0x0030
/** @} */

/** @brief Frame number mask (bit 0 of STATUS_REG). */
#define MLX90640_STAT_FRAME_MASK BIT_MASK(0)
/** @brief Extract current frame number from STATUS_REG value. */
#define MLX90640_GET_FRAME(reg_value) (reg_value & MLX90640_STAT_FRAME_MASK)
/** @brief Data-ready mask (bit 3 of STATUS_REG). */
#define MLX90640_STAT_DATA_READY_MASK BIT_MASK(3)
/** @brief Extract data-ready flag from STATUS_REG value. */
#define MLX90640_GET_DATA_READY(reg_value) (reg_value & MLX90640_STAT_DATA_READY_MASK)

/** @name Control register 0x800D bit fields
 *  @{ */
#define MLX90640_CTRL_REG 0x800D
/** @brief Trigger-ready mask (bit 15). Used in trigger-measurement tests. */
#define MLX90640_CTRL_TRIG_READY_MASK BIT_MASK(15)
/** @brief Bit position of the refresh-rate field in CTRL_REG. */
#define MLX90640_CTRL_REFRESH_SHIFT 7
/** @brief Mask for the 3-bit refresh-rate field (bits [9:7]). */
#define MLX90640_CTRL_REFRESH_MASK REG_MASK(MLX90640_CTRL_REFRESH_SHIFT,3)
/** @brief Bit position of the ADC resolution field in CTRL_REG. */
#define MLX90640_CTRL_RESOLUTION_SHIFT 10
/** @brief Mask for the 2-bit resolution field (bits [11:10]). */
#define MLX90640_CTRL_RESOLUTION_MASK REG_MASK(MLX90640_CTRL_RESOLUTION_SHIFT,2)
/** @brief Bit position of the measurement-mode field in CTRL_REG. */
#define MLX90640_CTRL_MEAS_MODE_SHIFT 12
/** @brief Mask for the measurement-mode bit (bit 12). */
#define MLX90640_CTRL_MEAS_MODE_MASK BIT_MASK(12)
/** @} */

/** @name Word/byte/nibble extraction helpers
 *  Used when unpacking the packed EEPROM calibration words.
 *  @{ */
#define MLX90640_MS_BYTE_SHIFT 8
#define MLX90640_MS_BYTE_MASK 0xFF00
#define MLX90640_LS_BYTE_MASK 0x00FF
/** @brief Extract the most-significant byte of a 16-bit word. */
#define MLX90640_MS_BYTE(reg16) ((reg16 & MLX90640_MS_BYTE_MASK) >> MLX90640_MS_BYTE_SHIFT)
/** @brief Extract the least-significant byte of a 16-bit word. */
#define MLX90640_LS_BYTE(reg16) (reg16 & MLX90640_LS_BYTE_MASK)
#define MLX90640_MSBITS_6_MASK 0xFC00
#define MLX90640_LSBITS_10_MASK 0x03FF
#define MLX90640_NIBBLE1_MASK 0x000F
#define MLX90640_NIBBLE2_MASK 0x00F0
#define MLX90640_NIBBLE3_MASK 0x0F00
#define MLX90640_NIBBLE4_MASK 0xF000
/** @brief Extract nibble 1 (bits [3:0]). */
#define MLX90640_NIBBLE1(reg16) ((reg16 & MLX90640_NIBBLE1_MASK))
/** @brief Extract nibble 2 (bits [7:4]). */
#define MLX90640_NIBBLE2(reg16) ((reg16 & MLX90640_NIBBLE2_MASK) >> 4)
/** @brief Extract nibble 3 (bits [11:8]). */
#define MLX90640_NIBBLE3(reg16) ((reg16 & MLX90640_NIBBLE3_MASK) >> 8)
/** @brief Extract nibble 4 (bits [15:12]). */
#define MLX90640_NIBBLE4(reg16) ((reg16 & MLX90640_NIBBLE4_MASK) >> 12)
/** @} */

/** @brief 2 raised to the power @p x (floating point). */
#define POW2(x) pow(2, (double)x)

/** @brief Absolute alpha scaling factor defined by the datasheet (1e-6). */
#define SCALEALPHA 0.000001
    
/**
 * @brief Calibration parameters extracted from the device EEPROM.
 *
 * Filled by MLX90640_ExtractParameters() and consumed by the frame
 * calculations (MLX90640_CalculateTo / MLX90640_GetImage / GetVdd / GetTa).
 * Per-pixel arrays hold one entry for each of the 768 pixels.
 */
typedef struct
    {
        int16_t kVdd;               /**< Vdd calculation coefficient */
        int16_t vdd25;              /**< Vdd at 25 degC reference */
        float KvPTAT;               /**< PTAT supply-voltage coefficient */
        float KtPTAT;               /**< PTAT temperature coefficient */
        uint16_t vPTAT25;           /**< PTAT raw value at 25 degC */
        float alphaPTAT;            /**< PTAT alpha coefficient */
        int16_t gainEE;             /**< EEPROM gain calibration word */
        float tgc;                  /**< Thermal gradient coefficient */
        float cpKv;                 /**< Compensation-pixel Kv */
        float cpKta;                /**< Compensation-pixel Kta */
        uint8_t resolutionEE;       /**< ADC resolution stored in EEPROM */
        uint8_t calibrationModeEE;  /**< EEPROM calibration mode (interleaved/chess) */
        float KsTa;                 /**< Ambient-temperature coefficient */
        float ksTo[5];              /**< KsTo coefficients per temperature range */
        int16_t ct[5];              /**< Corner temperatures of the To ranges */
        uint16_t alpha[768];        /**< Per-pixel sensitivity (scaled integer) */
        uint8_t alphaScale;         /**< Scale factor applied to alpha[] */
        int16_t offset[768];        /**< Per-pixel offset */
        int8_t kta[768];            /**< Per-pixel Kta (scaled integer) */
        uint8_t ktaScale;           /**< Scale factor applied to kta[] */
        int8_t kv[768];             /**< Per-pixel Kv (scaled integer) */
        uint8_t kvScale;            /**< Scale factor applied to kv[] */
        float cpAlpha[2];           /**< Compensation-pixel alpha (sub-page 0/1) */
        int16_t cpOffset[2];        /**< Compensation-pixel offset (sub-page 0/1) */
        float ilChessC[3];          /**< Interleaved/chess pattern correction coefficients */
        uint16_t brokenPixels[5];   /**< Indices of broken pixels (0xFFFF terminated) */
        uint16_t outlierPixels[5];  /**< Indices of outlier pixels (0xFFFF terminated) */
    } paramsMLX90640;

    /**
     * @brief Dump the full EEPROM calibration data.
     * @param[in]  slaveAddr 7-bit I2C slave address (0x33).
     * @param[out] eeData    Buffer of MLX90640_EEPROM_DUMP_NUM (832) words.
     * @return MLX90640_NO_ERROR on success, negative error code otherwise.
     */
    int MLX90640_DumpEE(uint8_t slaveAddr, uint16_t *eeData);

    /**
     * @brief Wait until the sensor signals a new frame is ready.
     *
     * Clears the data-ready flag and polls the status register. A local
     * timeout (~3000 polls) was added to avoid blocking forever when the
     * sensor ACKs on I2C but does not produce frames.
     * @param[in] slaveAddr 7-bit I2C slave address.
     * @return MLX90640_NO_ERROR on success, negative error code otherwise.
     */
    int MLX90640_SynchFrame(uint8_t slaveAddr);

    /**
     * @brief Trigger a measurement via software (device-triggered mode).
     * @param[in] slaveAddr 7-bit I2C slave address.
     * @return MLX90640_NO_ERROR on success, negative error code otherwise.
     */
    int MLX90640_TriggerMeasurement(uint8_t slaveAddr);

    /**
     * @brief Read one complete frame of raw data from the sensor RAM.
     *
     * Waits for the data-ready flag, reads the 768 pixel words plus the 64
     * auxiliary words, and appends the control register and sub-page number
     * at frameData[832] / frameData[833].
     * @param[in]  slaveAddr 7-bit I2C slave address.
     * @param[out] frameData Buffer of at least 834 words.
     * @return Sub-page number (0 or 1) on success, negative error code on failure.
     */
    int MLX90640_GetFrameData(uint8_t slaveAddr, uint16_t *frameData);

    /**
     * @brief Extract all calibration parameters from an EEPROM dump.
     * @param[in]  eeData    EEPROM words read by MLX90640_DumpEE().
     * @param[out] mlx90640  Parameter struct to fill.
     * @return MLX90640_NO_ERROR, or a bad-pixel warning/error code.
     */
    int MLX90640_ExtractParameters(uint16_t *eeData, paramsMLX90640 *mlx90640);

    /**
     * @brief Calculate the supply voltage from the auxiliary frame data.
     * @param[in] frameData Frame read by MLX90640_GetFrameData().
     * @param[in] params    Calibration parameters.
     * @return Vdd in volts.
     */
    float MLX90640_GetVdd(uint16_t *frameData, const paramsMLX90640 *params);

    /**
     * @brief Calculate the ambient (sensor die) temperature.
     * @param[in] frameData Frame read by MLX90640_GetFrameData().
     * @param[in] params    Calibration parameters.
     * @return Ta in degrees Celsius.
     */
    float MLX90640_GetTa(uint16_t *frameData, const paramsMLX90640 *params);

    /**
     * @brief Calculate the IR image (radiometric signal, not temperature).
     * @param[in]  frameData Frame read by MLX90640_GetFrameData().
     * @param[in]  params    Calibration parameters.
     * @param[out] result    Buffer of 768 floats receiving the image values.
     */
    void MLX90640_GetImage(uint16_t *frameData, const paramsMLX90640 *params, float *result);

    /**
     * @brief Calculate the object temperature (To) for every pixel.
     * @param[in]  frameData  Frame read by MLX90640_GetFrameData().
     * @param[in]  params     Calibration parameters.
     * @param[in]  emissivity Object emissivity (0..1, typically 0.95).
     * @param[in]  tr         Reflected temperature in degC (usually Ta - 8).
     * @param[out] result     Buffer of 768 floats receiving temperatures in degC.
     */
    void MLX90640_CalculateTo(uint16_t *frameData, const paramsMLX90640 *params, float emissivity, float tr, float *result);

    /**
     * @brief Set the ADC resolution (0..3 = 16..19 bit).
     * @param[in] slaveAddr  7-bit I2C slave address.
     * @param[in] resolution Resolution value written to CTRL_REG [11:10].
     * @return MLX90640_NO_ERROR on success, negative error code otherwise.
     */
    int MLX90640_SetResolution(uint8_t slaveAddr, uint8_t resolution);

    /**
     * @brief Read the current ADC resolution from CTRL_REG.
     * @param[in] slaveAddr 7-bit I2C slave address.
     * @return Resolution value (0..3) or negative error code.
     */
    int MLX90640_GetCurResolution(uint8_t slaveAddr);

    /**
     * @brief Set the frame refresh rate.
     *
     * Datasheet coding for CTRL_REG bits [9:7]:
     * 0=0.25 Hz, 1=0.5 Hz, 2=1 Hz, 3=2 Hz, 4=4 Hz, 5=8 Hz, 6=16 Hz, 7=32 Hz.
     * @param[in] slaveAddr   7-bit I2C slave address.
     * @param[in] refreshRate 3-bit refresh-rate code.
     * @return MLX90640_NO_ERROR on success, negative error code otherwise.
     */
    int MLX90640_SetRefreshRate(uint8_t slaveAddr, uint8_t refreshRate);

    /**
     * @brief Read the current refresh-rate code from CTRL_REG.
     * @param[in] slaveAddr 7-bit I2C slave address.
     * @return Refresh-rate code (0..7) or negative error code.
     */
    int MLX90640_GetRefreshRate(uint8_t slaveAddr);

    /**
     * @brief Get the sub-page number (0 or 1) of the given frame.
     * @param[in] frameData Frame read by MLX90640_GetFrameData().
     * @return Sub-page number stored in frameData[833].
     */
    int MLX90640_GetSubPageNumber(uint16_t *frameData);

    /**
     * @brief Read the current measurement mode (interleaved/chess).
     * @param[in] slaveAddr 7-bit I2C slave address.
     * @return Mode value or negative error code.
     */
    int MLX90640_GetCurMode(uint8_t slaveAddr);

    /**
     * @brief Select interleaved readout mode.
     * @param[in] slaveAddr 7-bit I2C slave address.
     * @return MLX90640_NO_ERROR on success, negative error code otherwise.
     */
    int MLX90640_SetInterleavedMode(uint8_t slaveAddr);

    /**
     * @brief Select chess-pattern readout mode (factory default, better accuracy).
     * @param[in] slaveAddr 7-bit I2C slave address.
     * @return MLX90640_NO_ERROR on success, negative error code otherwise.
     */
    int MLX90640_SetChessMode(uint8_t slaveAddr);

    /**
     * @brief Replace bad pixels (broken/outlier) by neighbour interpolation.
     * @param[in]    pixels 0xFFFF-terminated list of bad pixel indices.
     * @param[inout] to     Temperature array to correct in place.
     * @param[in]    mode   0 = line-neighbour mode, 1 = diagonal mode.
     * @param[in]    params Calibration parameters (for bad-pixel lookup).
     */
    void MLX90640_BadPixelsCorrection(uint16_t *pixels, float *to, int mode, paramsMLX90640 *params);

/** @} (end of mlx90640_api) */

#endif
