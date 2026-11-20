/*********************************************************************
 * File Name          : spi.c
 * Version            : V1.0.0
 * Description        : LCD software SPI (CH585).
 *********************************************************************************
* Attention: CH585 hardware SPI0 can only use PA12-PA15 or PB12-PB15,
*            so the LCD SCLK/MOSI on PB5/PB2 are driven by software SPI.
*******************************************************************************/
#include "spi.h"

/*********************************************************************
 * @fn      SPI2_Init
 *
 * @brief   Initialize the LCD software SPI pins (SCLK idle low, mode 0)
 *
 * @return  none
 */
void SPI2_Init(void)
{
    GPIOB_ResetBits(LCD_SCL_PIN);
    GPIOB_SetBits(LCD_MOSI_PIN);
    GPIOB_ModeCfg(LCD_SCL_PIN | LCD_MOSI_PIN, GPIO_ModeOut_PP_5mA);
}

/*********************************************************************
 * @fn      SPI2_Write
 *
 * @brief   Send one byte, MSB first, mode 0
 *
 * @param   data - the data to send
 *
 * @return  none
 */
void SPI2_Write(uint8_t data)
{
    LCD_SPI_TX_BYTE(data);
}
