/*********************************************************************
 * File Name          : main.c
 * Author             : 混分巨兽龙某某
 * Version            : V1.0.0
 * Date               : 2026/4/29
 * Description        : Main program body.
*******************************************************************************/

/*
 *@Note
  iKun_Power_Analyze: USBSS Current Voltage Power Meter Code
  pins: 
    I2C_SDA    -- PA0
    I2C_SCL    -- PA1

    LCD_LED    -- PB9
    LCD_DC     -- PB10
    LCD_RST    -- PB11
    LCD_CS     -- PB12
    LCD_SCK    -- PB13(SPI2_SCK)
    LCD_MOSI   -- PB15(SPI2_MOSI)

*/

#include "debug.h"
#include "string.h"
#include "lcd.h"
#include "config.h"
#include "i2c_ina226.h"
#include "image.h"
#include <stdio.h>
#include "curve.h"
#include "ch32v20x_usbfs_device.h"

// Define curve area
CurveArea voltage_curve;

/**
 * @brief Send data via USB CDC
 * @param pbuf - Pointer to data buffer
 * @param len  - Length of data to send
 * @return 1 if data sent successfully, 0 if endpoint is busy
 */
uint8_t USB_CDC_SendData(uint8_t *pbuf, uint16_t len) {
    if(USBFS_Endp_Busy[DEF_UEP3] == 0) {
        USBFS_Endp_DataUp(DEF_UEP3, pbuf, len, DEF_UEP_CPY_LOAD);
        return 1;
    }
    return 0;
}

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{
    char usb_send_buf[32];

    SystemCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);
    printf("SystemClk:%d\r\n", SystemCoreClock);
    printf("iKun_Power_Analyze_V1.0\r\n");
    printf("Author: iKun Developer\r\n");

    USBFS_RCC_Init();
    USBFS_Device_Init(ENABLE);
    NVIC_EnableIRQ(USBFS_IRQn);

    I2C1_Init();
    INA226_Init();
    LCD_Init();
    LCD_Clear(LCD_BLACK);
    Showimage(gImage_ikun);
    Delay_Ms(1000);
    LCD_Clear(LCD_BLACK);

    Showimage_40_U(gImage_U);
    Showimage_40_I(gImage_I);
    Showimage_40_P(gImage_P);
    Showimage_ikun(gImage_ikun2);

    LCD_DrawBigChar_48x48(260, 7, 'V', LCD_RED);
    LCD_DrawBigChar_48x48(260, 55, 'A', LCD_GREEN);
    LCD_DrawBigChar_48x48(260, 105, 'W', LCD_YELLOW);

    Curve_Init(&voltage_curve, 10, 155, 240, 80, 0, 220, LCD_RED, LCD_BLACK);
    voltage_curve.grid_color = LCD_GRAY;
    voltage_curve.draw_grid = 1;

    while (1)
    {
        float bus_v   = INA226_Read_BusVoltage();
        float current = INA226_Read_Current();
        float power   = INA226_Read_Power();
        int16_t current_mA = (int16_t)(current * 1000);
        Curve_AddPoint(&voltage_curve, current_mA);

        sprintf(usb_send_buf, "U: %.4f V; I: %.4f A; P: %.4f W\r\n", bus_v, current, power);
        USB_CDC_SendData((uint8_t *)usb_send_buf, strlen(usb_send_buf));

        LCD_ShowBigFloat_24x48(65, 5, bus_v, 4, LCD_RED, 3);
        LCD_ShowBigFloat_24x48(65, 55, current, 4, LCD_GREEN, 3);
        LCD_ShowBigFloat_24x48(65, 105, power, 4, LCD_YELLOW, 3);

        Delay_Ms(50);
    }
}
