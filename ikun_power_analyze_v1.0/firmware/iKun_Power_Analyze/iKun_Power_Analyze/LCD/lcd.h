/*********************************************************************
 * File Name          : lcd.h
 * Author             : ��־�����ĳĳ
 * Version            : V1.0.0
 * Date               : 2023/8/15
 * Description        : file for lcd screen.
 *********************************************************************************
* Copyright (c) 2023 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for 
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef __LCD_H
#define __LCD_H
#include "spi.h"
#include "stdio.h"
#include "stdlib.h"
#include "config.h"

// [15:11] 5bit red
//  [10:5] 6bit green
//   [4:0] 5bit blue
#define LCD_WHITE    0xFFFF
#define LCD_BLACK    0x0000
#define LCD_BLUE     0x001F
#define LCD_BRED     0XF81F
#define LCD_GRED     0XFFE0
#define LCD_GBLUE    0X07FF
#define LCD_RED      0xF800
#define LCD_MAGENTA  0xF81F
#define LCD_GREEN    0x07E0
#define LCD_CYAN     0x7FFF
#define LCD_YELLOW   0xFFE0
#define LCD_BROWN    0XBC40
#define LCD_BRRED    0XFC07
#define LCD_GRAY     0X8430

#define LCD_LED_SET  (LCD_LED_PORT->BSHR = LCD_LED_PIN)
#define LCD_CS_SET   (LCD_CS_PORT->BSHR = LCD_CS_PIN)
#define LCD_DC_SET   (LCD_DC_PORT->BSHR = LCD_DC_PIN)
#define LCD_RST_SET  (LCD_RST_PORT->BSHR = LCD_RST_PIN)

#define LCD_LED_CLR  (LCD_LED_PORT->BCR = LCD_LED_PIN)
#define LCD_CS_CLR   (LCD_CS_PORT->BCR = LCD_CS_PIN)
#define LCD_DC_CLR   (LCD_DC_PORT->BCR = LCD_DC_PIN)
#define LCD_RST_CLR  (LCD_RST_PORT->BCR = LCD_RST_PIN)

void LCD_Init(void);
void LCD_Clear(uint16_t Color);

void Lcd_WriteData_16Bit(uint16_t Data);
void LCD_direction(uint8_t direction);
void LCD_SetWindows(uint16_t xStar, uint16_t yStar, uint16_t xEnd, uint16_t yEnd);

void LCD_drawImageWithSize(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t addr);

void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void LCD_FillRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void LCD_DrawCircle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);
void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t ch, uint16_t color, uint16_t bk_color);
void LCD_ShowString(uint16_t x, uint16_t y, uint8_t *str, uint16_t color, uint16_t bk_color);
void LCD_ShowNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint16_t color, uint16_t bk_color);
void Gui_DrawFont_GBK16(u16 x, u16 y, u16 fc, u16 bc, u8 *s);
void Address_set(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2);
void Showimage(const unsigned char *p);
void Showimage_40_U(const unsigned char *p);
void Showimage_40_I(const unsigned char *p);
void Showimage_40_P(const unsigned char *p);
void Showimage_ikun(const unsigned char *p);
void LCD_ShowBigFloat_24x48_Fast(uint16_t x, uint16_t y, float num, uint8_t dec, uint16_t color, uint8_t spacing);
void LCD_DrawBigChar_48x48(uint16_t x, uint16_t y, char c, uint16_t color);
void LCD_DrawBigChar_24x48_Fast(uint16_t x, uint16_t y, char c, uint16_t color);

/* New function declarations */
void FloatToStr(float num, char *buf, uint8_t dec);
void LCD_DrawBigChar_24x48(uint16_t x, uint16_t y, char c, uint16_t color);
void LCD_Fill_Rect_Fast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void LCD_ShowBigFloat_24x48(uint16_t x, uint16_t y, float num, uint8_t dec, uint16_t color, uint8_t spacing);
void LCD_Fill_Rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void LCD_ShowBigFloat_24x48_Clear(uint16_t x, uint16_t y, float num, uint8_t dec, uint16_t color, uint8_t spacing, uint16_t bgColor);

#endif
