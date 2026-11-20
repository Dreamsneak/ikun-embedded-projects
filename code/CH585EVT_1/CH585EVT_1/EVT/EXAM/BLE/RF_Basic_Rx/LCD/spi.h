/*********************************************************************
 * File Name          : spi.h
 * Version            : V1.0.0
 * Description        : LCD software SPI (CH585).
 *********************************************************************************
* Attention: CH585 hardware SPI0 can only use PA12-PA15 or PB12-PB15,
*            so the LCD SCLK/MOSI on PB5/PB2 are driven by software SPI.
*******************************************************************************/
#ifndef __SPI_H__
#define __SPI_H__

#include "CH58x_common.h"

/* LCD software SPI pins */
#define LCD_SCL_PIN     GPIO_Pin_5
#define LCD_MOSI_PIN    GPIO_Pin_2

/* Send one bit (mode 0, MSB first):
 * MOSI is updated while SCL is low, then one rising edge latches it.
 * Writing 0 to the SET/CLR register has no effect, so no branch is needed. */
#define LCD_SPI_TX_BIT(v, n)                                    \
    do {                                                        \
        R32_PB_CLR = LCD_MOSI_PIN;                              \
        R32_PB_SET = (((uint32_t)(v) << (n)) & 0x80u) >> 5;     \
        R32_PB_SET = LCD_SCL_PIN;                               \
        R32_PB_CLR = LCD_SCL_PIN;                               \
    } while(0)

/* Unrolled 8-bit transfer, removes loop/branch overhead of the per-bit path */
#define LCD_SPI_TX_BYTE(d)                                      \
    do {                                                        \
        uint8_t _sd = (uint8_t)(d);                             \
        LCD_SPI_TX_BIT(_sd, 0);                                 \
        LCD_SPI_TX_BIT(_sd, 1);                                 \
        LCD_SPI_TX_BIT(_sd, 2);                                 \
        LCD_SPI_TX_BIT(_sd, 3);                                 \
        LCD_SPI_TX_BIT(_sd, 4);                                 \
        LCD_SPI_TX_BIT(_sd, 5);                                 \
        LCD_SPI_TX_BIT(_sd, 6);                                 \
        LCD_SPI_TX_BIT(_sd, 7);                                 \
    } while(0)

/* Inlined 16-bit pixel write, used by all bulk drawing loops */
static inline __attribute__((always_inline)) void SPI2_Write16(uint16_t data)
{
    LCD_SPI_TX_BYTE((uint8_t)(data >> 8));
    LCD_SPI_TX_BYTE((uint8_t)data);
}

void SPI2_Init(void);
void SPI2_Write(uint8_t data);

#endif
