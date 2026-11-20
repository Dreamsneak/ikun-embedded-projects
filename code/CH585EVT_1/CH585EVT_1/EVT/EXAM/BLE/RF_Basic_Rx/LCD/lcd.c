/*********************************************************************
 * File Name          : lcd.c
 * Author             : 混分巨兽龙某某
 * Version            : V1.0.0
 * Date               : 2026/4/29
 * Description        : LCD screen driver functions.
*******************************************************************************/

#include "lcd.h"
#include "font.h"
#include <string.h>

#define FONT_WIDTH  8
#define FONT_HEIGHT 16

static uint16_t width;
static uint16_t height;

#define LCD_SOFT_RESET          (0x01)
#define LCD_READ_ID             (0x04)
#define LCD_READ_DISPLAY_STATUS (0x09)
#define LCD_ON                  (0x28)
#define LCD_OFF                 (0x29)

#define LCD_SET_X               (0x2a)
#define LCD_SET_Y               (0x2B)
#define LCD_MEM_WRITE           (0x2C)
#define LCD_MEM_READ            (0x2e)

/*********************************************************************
 * @fn      LCD_WR_REG
 *
 * @brief   Write an 8-bit command to the LCD screen
 * 
 * @param   data - Command value to be written
 *
 * @return  none
 */
static void LCD_WR_REG(uint8_t data)
{
    LCD_CS_CLR;
    LCD_DC_CLR;
    SPI2_Write(data);
    LCD_CS_SET;
}

/*********************************************************************
 * @fn      LCD_WR_DATA
 *
 * @brief   Write an 8-bit data to the LCD screen
 * 
 * @param   data - the data to write
 * 
 */
static void LCD_WR_DATA(uint8_t data)
{
    LCD_CS_CLR;
    LCD_DC_SET;
    SPI2_Write(data);
    LCD_CS_SET;
}

/*********************************************************************
 * @fn      LCD_WriteReg
 * 
 * @brief   write a data to the register
 * 
 * @param   LCD_Reg - register address
 *          LCD_RegValue - the data to write
 * 
 * @return  none
 */
static void LCD_WriteReg(uint8_t LCD_Reg, uint8_t LCD_RegValue)
{
    LCD_WR_REG(LCD_Reg);
    LCD_WR_DATA(LCD_RegValue);
}

/*********************************************************************
 * @fn      Lcd_WriteData_16Bit
 * 
 * @brief   write two bytes to the lcd screen
 * 
 * @param   Data - the data to write
 * 
 * @return  none
 */
void Lcd_WriteData_16Bit(uint16_t Data)
{
    LCD_CS_CLR;
    LCD_DC_SET;            // data mode
    SPI2_Write16(Data);
    LCD_CS_SET;
}

/*********************************************************************
 * @fn      LCD_GPIOInit
 * 
 * @brief   Configuring the control gpio for the lcd screen
 * 
 * @return  none
 */
void LCD_GPIOInit(void)
{
    LCD_LED_SET;
    LCD_CS_SET;
    LCD_DC_SET;
    LCD_RST_SET;

    GPIOB_ModeCfg(LCD_LED_PIN | LCD_DC_PIN | LCD_RST_PIN | LCD_CS_PIN, GPIO_ModeOut_PP_5mA);
}

/*********************************************************************
 * @fn      LCD_RESET
 * 
 * @brief   reset the lcd screen by the rst pin
 * 
 * @return  none
 */
void LCD_RESET(void)
{
    LCD_RST_CLR;
    DelayMs(100);
    LCD_RST_SET;
    DelayMs(50);
}

/*********************************************************************
 * @fn      LCD_Init
 * 
 * @brief   Initialization LCD screen
 * 
 * @return  none
 */
void LCD_Init(void)
{
    SPI2_Init();
    LCD_GPIOInit();
    LCD_RESET();
    LCD_LED_SET;

    LCD_WR_REG(0xCF);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0xC9);  //C1
    LCD_WR_DATA(0X30);
    LCD_WR_REG(0xED);
    LCD_WR_DATA(0x64);
    LCD_WR_DATA(0x03);
    LCD_WR_DATA(0X12);
    LCD_WR_DATA(0X81);
    LCD_WR_REG(0xE8);
    LCD_WR_DATA(0x85);
    LCD_WR_DATA(0x10);
    LCD_WR_DATA(0x7A);
    LCD_WR_REG(0xCB);
    LCD_WR_DATA(0x39);
    LCD_WR_DATA(0x2C);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x34);
    LCD_WR_DATA(0x02);
    LCD_WR_REG(0xF7);
    LCD_WR_DATA(0x20);
    LCD_WR_REG(0xEA);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x00);
    LCD_WR_REG(0xC0);   //Power control
    LCD_WR_DATA(0x1B);  //VRH[5:0]
    LCD_WR_REG(0xC1);   //Power control
    LCD_WR_DATA(0x00);  //SAP[2:0];BT[3:0] 01
    LCD_WR_REG(0xC5);   //VCM control
    LCD_WR_DATA(0x30);  //3F
    LCD_WR_DATA(0x30);  //3C
    LCD_WR_REG(0xC7);   //VCM control2
    LCD_WR_DATA(0XB7);
    LCD_WR_REG(0x3A);
    LCD_WR_DATA(0x55);
    LCD_WR_REG(0xB1);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x1A);
    LCD_WR_REG(0xB6);  // Display Function Control
    LCD_WR_DATA(0x0A);
    LCD_WR_DATA(0xA2);
    LCD_WR_REG(0xF2);  // 3Gamma Function Disable
    LCD_WR_DATA(0x00);
    LCD_WR_REG(0x26);  //Gamma curve selected
    LCD_WR_DATA(0x01);
    LCD_WR_REG(0xE0);  //Set Gamma
    LCD_WR_DATA(0x0F);
    LCD_WR_DATA(0x2A);
    LCD_WR_DATA(0x28);
    LCD_WR_DATA(0x08);
    LCD_WR_DATA(0x0E);
    LCD_WR_DATA(0x08);
    LCD_WR_DATA(0x54);
    LCD_WR_DATA(0XA9);
    LCD_WR_DATA(0x43);
    LCD_WR_DATA(0x0A);
    LCD_WR_DATA(0x0F);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x00);
    LCD_WR_REG(0XE1);  //Set Gamma
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x15);
    LCD_WR_DATA(0x17);
    LCD_WR_DATA(0x07);
    LCD_WR_DATA(0x11);
    LCD_WR_DATA(0x06);
    LCD_WR_DATA(0x2B);
    LCD_WR_DATA(0x56);
    LCD_WR_DATA(0x3C);
    LCD_WR_DATA(0x05);
    LCD_WR_DATA(0x10);
    LCD_WR_DATA(0x0F);
    LCD_WR_DATA(0x3F);
    LCD_WR_DATA(0x3F);
    LCD_WR_DATA(0x0F);
    LCD_WR_REG(0x2B);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x01);
    LCD_WR_DATA(0x3f);
    LCD_WR_REG(0x2A);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0xef);
    LCD_WR_REG(0x11);  //Exit Sleep
    DelayMs(120);

    LCD_direction(USE_HORIZONTAL);  

    LCD_WR_REG(0x29);  //display on
}

/*********************************************************************
 * @fn      LCD_SetWindows
 * 
 * @brief   Setting LCD display window
 * 
 * @param   xStar -  the bebinning x coordinate of the LCD display window
 *          yStar -  the bebinning y coordinate of the LCD display window
 *          xEnd -  the endning x coordinate of the LCD display window
 *          yEnd -  the endning y coordinate of the LCD display window
 * 
 * @return  none
 */
void LCD_SetWindows(uint16_t xStar, uint16_t yStar, uint16_t xEnd, uint16_t yEnd)
{
    LCD_CS_CLR;

    LCD_DC_CLR;
    SPI2_Write(LCD_SET_X);
    LCD_DC_SET;
    SPI2_Write(xStar >> 8);
    SPI2_Write(0x00FF & xStar);
    SPI2_Write(xEnd >> 8);
    SPI2_Write(0x00FF & xEnd);

    LCD_DC_CLR;
    SPI2_Write(LCD_SET_Y);
    LCD_DC_SET;
    SPI2_Write(yStar >> 8);
    SPI2_Write(0x00FF & yStar);
    SPI2_Write(yEnd >> 8);
    SPI2_Write(0x00FF & yEnd);

    LCD_DC_CLR;
    SPI2_Write(LCD_MEM_WRITE);
}

/*********************************************************************
 * @fn      LCD_direction
 * 
 * @brief   Setting the display direction of LCD screen
 * 
 * @param   direction -   direction:0-0 degree  1-90 degree  2-180 degree  3-270 degree
 * 
 * @return  none
 */
void LCD_direction(uint8_t direction)
{
    switch (direction)
    {
        case 0:
            width  = LCD_W;
            height = LCD_H;
            LCD_WriteReg(0x36, (1 << 3) | (0 << 6) | (0 << 7));  //BGR==1,MY==0,MX==0,MV==0
            break;
        case 1:
            width  = LCD_H;
            height = LCD_W;
            LCD_WriteReg(0x36, (1 << 3) | (0 << 7) | (1 << 6) | (1 << 5));  //BGR==1,MY==1,MX==0,MV==1
            break;
        case 2:
            width  = LCD_W;
            height = LCD_H;
            LCD_WriteReg(0x36, (1 << 3) | (1 << 6) | (1 << 7));  //BGR==1,MY==0,MX==0,MV==0
            break;
        case 3:
            width  = LCD_H;
            height = LCD_W;
            LCD_WriteReg(0x36, (1 << 3) | (1 << 7) | (1 << 5));  //BGR==1,MY==1,MX==0,MV==1
            break;
        default:
            break;
    }
}

/*********************************************************************
 * @fn      LCD_Clear
 * 
 * @brief   Full screen filled LCD screen
 * 
 * @param   Color -   Filled color
 * 
 * @return  none
 */
void LCD_Clear(uint16_t Color)
{
    uint32_t i, n;

    LCD_SetWindows(0, 0, width - 1, height - 1);
    LCD_DC_SET;

    n = (uint32_t)width * height;
    for(i = 0; i < n; i++)
    {
        SPI2_Write16(Color);
    }
    LCD_CS_SET;
}

/**
 * @brief Draw a single point on LCD
 * @param x     X coordinate
 * @param y     Y coordinate
 * @param color Point color
 */
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    // Check boundary to prevent out of screen
    if (x >= width || y >= height)
        return;

    // Set window to single point
    LCD_SetWindows(x, y, x, y);

    LCD_DC_SET;          // data mode
    SPI2_Write16(color); // write 16-bit color data
    LCD_CS_SET;          // CS high, end communication
}

/**
 * @brief Draw a line using Bresenham algorithm
 * @param x1    Start X coordinate
 * @param y1    Start Y coordinate
 * @param x2    End X coordinate
 * @param y2    End Y coordinate
 * @param color Line color
 */
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    int16_t dx, dy, sx, sy, err, e2;

    dx = (x1 < x2) ? (x2 - x1) : (x1 - x2);  // X distance
    dy = (y1 < y2) ? (y2 - y1) : (y1 - y2);  // Y distance
    sx = (x1 < x2) ? 1 : -1;                 // X step direction
    sy = (y1 < y2) ? 1 : -1;                 // Y step direction
    err = dx - dy;                            // error term

    while (1)
    {
        LCD_DrawPoint(x1, y1, color);        // draw current point

        // Reached end point, exit loop
        if (x1 == x2 && y1 == y2)
            break;

        e2 = 2 * err;

        // X step
        if (e2 > -dy)
        {
            err -= dy;
            x1 += sx;
        }

        // Y step
        if (e2 < dx)
        {
            err += dx;
            y1 += sy;
        }
    }
}

/**
 * @brief Draw a rectangle outline
 * @param x1    Top-left X coordinate
 * @param y1    Top-left Y coordinate
 * @param x2    Bottom-right X coordinate
 * @param y2    Bottom-right Y coordinate
 * @param color Border color
 */
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    LCD_DrawLine(x1, y1, x2, y1, color);  // top
    LCD_DrawLine(x2, y1, x2, y2, color);  // right
    LCD_DrawLine(x2, y2, x1, y2, color);  // bottom
    LCD_DrawLine(x1, y2, x1, y1, color);  // left
}

/**
 * @brief Fill a rectangle with color
 * @param x1    Start X coordinate
 * @param y1    Start Y coordinate
 * @param x2    End X coordinate
 * @param y2    End Y coordinate
 * @param color Fill color
 */
void LCD_FillRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint32_t i, n;
    uint16_t xs = (x1 < x2) ? x1 : x2;  // determine smaller X start
    uint16_t xe = (x1 > x2) ? x1 : x2;  // determine larger X end
    uint16_t ys = (y1 < y2) ? y1 : y2;  // determine smaller Y start
    uint16_t ye = (y1 > y2) ? y1 : y2;  // determine larger Y end

    if (xe >= width)
        xe = width - 1;
    if (ye >= height)
        ye = height - 1;
    if (xs > xe || ys > ye)
        return;

    LCD_SetWindows(xs, ys, xe, ye);
    LCD_DC_SET;

    n = (uint32_t)(xe - xs + 1) * (ye - ys + 1);
    for (i = 0; i < n; i++)
    {
        SPI2_Write16(color);
    }
    LCD_CS_SET;
}

/**
 * @brief Draw a circle using Bresenham circle algorithm
 * @param x0    X coordinate of circle center
 * @param y0    Y coordinate of circle center
 * @param r     Radius of the circle (unit: pixel)
 * @param color RGB565 color value for drawing
 */
void LCD_DrawCircle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color)
{
    int16_t x = 0, y = r, err = 1 - r;

    /* Use 8-way symmetry of circle, only calculate 1/8 area to optimize performance */
    while (x <= y)
    {
        LCD_DrawPoint(x0 + x, y0 + y, color);
        LCD_DrawPoint(x0 + y, y0 + x, color);
        LCD_DrawPoint(x0 - x, y0 + y, color);
        LCD_DrawPoint(x0 - y, y0 + x, color);
        LCD_DrawPoint(x0 + x, y0 - y, color);
        LCD_DrawPoint(x0 + y, y0 - x, color);
        LCD_DrawPoint(x0 - x, y0 - y, color);
        LCD_DrawPoint(x0 - y, y0 - x, color);

        /* Update error term and coordinate */
        if (err < 0)
        {
            err += 2 * x + 3;
        }
        else
        {
            err += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

/**
 * @brief Draw one 16-pixel-high glyph (8 or 16 wide) with a single window/CS cycle
 * @param bits MSB-first bitmap of the glyph, row by row
 */
static void lcd_draw_glyph(uint16_t x, uint16_t y, uint8_t w, const uint8_t *bits, uint16_t fc, uint16_t bc)
{
    uint8_t i, j;

    if (fc == bc)  /* transparent background: only draw foreground pixels */
    {
        for (i = 0; i < 16; i++)
        {
            for (j = 0; j < w; j++)
            {
                if (bits[i * (w >> 3) + (j >> 3)] & (0x80 >> (j & 7)))
                    LCD_DrawPoint(x + j, y + i, fc);
            }
        }
        return;
    }

    LCD_SetWindows(x, y, x + w - 1, y + 15);
    LCD_DC_SET;
    for (i = 0; i < 16; i++)
    {
        for (j = 0; j < w; j++)
        {
            SPI2_Write16((bits[i * (w >> 3) + (j >> 3)] & (0x80 >> (j & 7))) ? fc : bc);
        }
    }
    LCD_CS_SET;
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
    unsigned short k;
    uint16_t x_start = x;  /* Record original X position for line feed */

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

            /* Draw 16*8 ASCII dot matrix font, one window per glyph */
            lcd_draw_glyph(x, y, 8, &asc16[k * 16], fc, bc);
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
                    /* Draw 16*16 Chinese dot matrix font, one window per glyph */
                    lcd_draw_glyph(x, y, 16, (const uint8_t *)hz16[k].Msk, fc, bc);
                    break;  /* Exit loop once character is matched */
                }
            }
            s += 2;    /* GBK character occupies 2 bytes */
            x += 16;   /* Chinese character occupies 16 pixels width */
        }
    }
}

/**
 * @brief Set LCD display window address range
 * @param x1  Window start X coordinate
 * @param y1  Window start Y coordinate
 * @param x2  Window end X coordinate
 * @param y2  Window end Y coordinate
 * @note  LCD command: 0x2A(Column address), 0x2B(Page address), 0x2C(Write RAM)
 */
void Address_set(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2)
{
    /* Set column (X) address */
    SPI2_Write(0x2A);
    SPI2_Write(x1 >> 8);
    SPI2_Write(x1 & 0xFF);
    SPI2_Write(x2 >> 8);
    SPI2_Write(x2 & 0xFF);

    /* Set page (Y) address */
    SPI2_Write(0x2B);
    SPI2_Write(y1 >> 8);
    SPI2_Write(y1 & 0xFF);
    SPI2_Write(y2 >> 8);
    SPI2_Write(y2 & 0xFF);

    /* Enter LCD GRAM write mode */
    SPI2_Write(0x2C);
}

/**
 * @brief Blit an RGB565 bitmap (little-endian) with a single window/CS cycle
 */
static void lcd_blit_image(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const unsigned char *p)
{
    uint32_t i, n = (uint32_t)w * h;

    LCD_SetWindows(x, y, x + w - 1, y + h - 1);
    LCD_DC_SET;
    for (i = 0; i < n; i++)
    {
        SPI2_Write16((uint16_t)p[2 * i] | ((uint16_t)p[2 * i + 1] << 8));
    }
    LCD_CS_SET;
}

/**
 * @brief Display 99*100 RGB565 bitmap on specified LCD area
 * @param p Pointer to RGB565 image data buffer (little-endian format)
 */
void Showimage(const unsigned char *p)
{
    lcd_blit_image(110, 70, 99, 100, p);
}

/**
 * @brief Display 40*39 RGB565 bitmap (Position U)
 * @param p Pointer to RGB565 image data buffer
 */
void Showimage_40_U(const unsigned char *p)
{
    lcd_blit_image(10, 10, 40, 39, p);
}

/**
 * @brief Display 40*39 RGB565 bitmap (Position I)
 * @param p Pointer to RGB565 image data buffer
 */
void Showimage_40_I(const unsigned char *p)
{
    lcd_blit_image(10, 60, 40, 39, p);
}

/**
 * @brief Display 40*39 RGB565 bitmap (Position P)
 * @param p Pointer to RGB565 image data buffer
 */
void Showimage_40_P(const unsigned char *p)
{
    lcd_blit_image(10, 110, 40, 39, p);
}

void Showimage_ikun(const unsigned char *p)
{
    lcd_blit_image(260, 170, 49, 50, p);
}

/**
 * @brief Convert float to string with specified decimal places
 * @param num   Input float number
 * @param buf   Output string buffer (minimum 20 bytes recommended)
 * @param dec   Number of decimal places (1~4)
 */
void FloatToStr(float num, char *buf, uint8_t dec) {
    int sign = 0;
    if(num < 0) {
        sign = 1;
        num = -num;
    }
    
    int multiplier = 1;
    for(uint8_t i = 0; i < dec; i++) multiplier *= 10;
    
    long long scaled = (long long)(num * multiplier + 0.5);  // rounding
    
    int int_part = (int)(scaled / multiplier);
    int frac_part = (int)(scaled % multiplier);
    
    if(sign) {
        sprintf(buf, "-%02d.%0*d", int_part, dec, frac_part);
    } else {
        sprintf(buf, "%02d.%0*d", int_part, dec, frac_part);
    }
}

/**
 * @brief Draw a 24x48 large character
 * @param x     Top-left X coordinate (landscape mode)
 * @param y     Top-left Y coordinate
 * @param c     Character to display ('0'-'9', '.', '-')
 * @param color Character color
 */
void LCD_DrawBigChar_24x48(uint16_t x, uint16_t y, char c, uint16_t color) {
    uint8_t index;
    if(c >= '0' && c <= '9') index = c - '0';
    else if(c == '.') index = 10;
    else if(c == '-') index = 11;
    else return;

    const uint8_t *pData = BigFont_24x48[index];
    uint8_t i, j, byte;

    LCD_SetWindows(x, y, x + 23, y + 47);   // 24x48 window
    LCD_DC_SET;

    for(j = 0; j < 48; j++) {               // height 48
        for(i = 0; i < 24; i++) {           // width 24
            byte = pData[j * 3 + (i / 8)];  // 3 bytes per row
            SPI2_Write16((byte & (0x80 >> (i % 8))) ? color : LCD_BLACK);
        }
    }
    LCD_CS_SET;
}

/**
 * @brief Fast fill a rectangular area
 * @param x     Top-left X coordinate
 * @param y     Top-left Y coordinate
 * @param w     Width of rectangle
 * @param h     Height of rectangle
 * @param color Fill color
 */
void LCD_Fill_Rect_Fast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    uint32_t i, n = (uint32_t)w * h;

    LCD_SetWindows(x, y, x + w - 1, y + h - 1);
    LCD_DC_SET;
    for(i = 0; i < n; i++) {
        SPI2_Write16(color);
    }
    LCD_CS_SET;
}

/**
 * @brief Display a float number using 24x48 font
 * @param x       Start X coordinate
 * @param y       Start Y coordinate
 * @param num     Float number to display
 * @param dec     Number of decimal places (0-4)
 * @param color   Text color
 * @param spacing Character spacing in pixels (recommended 3-5)
 */
void LCD_ShowBigFloat_24x48(uint16_t x, uint16_t y, float num, uint8_t dec, uint16_t color, uint8_t spacing) {
    char buf[20] = {0};

    if (num < 0) num = 0.0f;

    if (dec == 0) {
        sprintf(buf, "%d", (int)num);
    } else if (dec == 1) {
        sprintf(buf, "%.1f", num);
    } else if (dec == 2) {
        sprintf(buf, "%.2f", num);
    } else if (dec == 3) {
        sprintf(buf, "%.3f", num);
    } else {
        sprintf(buf, "%.4f", num);
    }

    uint8_t len = 0;
    while (buf[len]) len++;

    uint16_t total_w = len * 24 + (len - 1) * spacing;
    LCD_Fill_Rect_Fast(x, y, total_w, 48, LCD_BLACK);

    uint16_t curX = x;
    for (uint8_t i = 0; i < len; i++) {
        LCD_DrawBigChar_24x48(curX, y, buf[i], color);
        curX += 24 + spacing;
    }
}

/**
 * @brief Fill a rectangular area
 * @param x     Top-left X coordinate
 * @param y     Top-left Y coordinate
 * @param w     Width of rectangle
 * @param h     Height of rectangle
 * @param color Fill color
 */
void LCD_Fill_Rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    LCD_Fill_Rect_Fast(x, y, w, h, color);
}

/**
 * @brief Display a float number with background clearing using 24x48 font
 * @param x       Start X coordinate
 * @param y       Start Y coordinate
 * @param num     Float number to display
 * @param dec     Number of decimal places
 * @param color   Text color
 * @param spacing Character spacing in pixels
 * @param bgColor Background color
 */
void LCD_ShowBigFloat_24x48_Clear(uint16_t x, uint16_t y, float num, uint8_t dec, uint16_t color, uint8_t spacing, uint16_t bgColor) {
    char buf[20];
    FloatToStr(num, buf, dec);
    uint8_t len = strlen(buf);
    uint16_t total_width = len * (24 + spacing);
    LCD_Fill_Rect(x, y, total_width, 48, bgColor);
    
    uint16_t curX = x;
    for(uint8_t i=0; i<len; i++) {
        LCD_DrawBigChar_24x48(curX, y, buf[i], color);
        curX += 24 + spacing;
    }
}

/**
 * @brief Fast draw a 24x48 large character
 * @param x     Top-left X coordinate
 * @param y     Top-left Y coordinate
 * @param c     Character to display ('0'-'9', '.', '-')
 * @param color Character color
 */
void LCD_DrawBigChar_24x48_Fast(uint16_t x, uint16_t y, char c, uint16_t color) {
    LCD_DrawBigChar_24x48(x, y, c, color);
}

/**
 * @brief 48x48 font data for characters 'V', 'A', 'W'
 *        (SimSun font, 48x48 pixels)
 */
const uint8_t BigFont_48x48[3][288] = {
    {
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x7F,0xFF,0xC0,0x00,0xFF,0xF8,0x07,0xFC,0x00,0x00,0x1F,0xC0,0x01,0xF8,0x00,0x00,
        0x0F,0x00,0x01,0xFC,0x00,0x00,0x0E,0x00,0x00,0xFC,0x00,0x00,0x0E,0x00,0x00,0xFE,
        0x00,0x00,0x1C,0x00,0x00,0x7E,0x00,0x00,0x1C,0x00,0x00,0x7E,0x00,0x00,0x38,0x00,
        0x00,0x3F,0x00,0x00,0x38,0x00,0x00,0x3F,0x00,0x00,0x70,0x00,0x00,0x1F,0x80,0x00,
        0x60,0x00,0x00,0x1F,0x80,0x00,0xE0,0x00,0x00,0x0F,0xC0,0x01,0xC0,0x00,0x00,0x0F,
        0xC0,0x01,0xC0,0x00,0x00,0x07,0xE0,0x03,0x80,0x00,0x00,0x07,0xE0,0x03,0x80,0x00,
        0x00,0x03,0xF0,0x07,0x00,0x00,0x00,0x03,0xF0,0x07,0x00,0x00,0x00,0x01,0xF8,0x0E,
        0x00,0x00,0x00,0x01,0xF8,0x0E,0x00,0x00,0x00,0x00,0xFC,0x1C,0x00,0x00,0x00,0x00,
        0xFC,0x18,0x00,0x00,0x00,0x00,0x7E,0x38,0x00,0x00,0x00,0x00,0x7E,0x30,0x00,0x00,
        0x00,0x00,0x3F,0x70,0x00,0x00,0x00,0x00,0x3F,0xE0,0x00,0x00,0x00,0x00,0x1F,0xE0,
        0x00,0x00,0x00,0x00,0x1F,0xC0,0x00,0x00,0x00,0x00,0x0F,0xC0,0x00,0x00,0x00,0x00,
        0x0F,0x80,0x00,0x00,0x00,0x00,0x07,0x80,0x00,0x00,0x00,0x00,0x07,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    },
    {
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x07,0xE0,0x00,0x00,0x00,0x00,0x0F,0xE0,0x00,0x00,0x00,0x00,0x0F,0xF0,
        0x00,0x00,0x00,0x00,0x1F,0xF0,0x00,0x00,0x00,0x00,0x1D,0xF8,0x00,0x00,0x00,0x00,
        0x39,0xF8,0x00,0x00,0x00,0x00,0x38,0xFC,0x00,0x00,0x00,0x00,0x70,0xFC,0x00,0x00,
        0x00,0x00,0xF0,0x7E,0x00,0x00,0x00,0x00,0xE0,0x7E,0x00,0x00,0x00,0x01,0xE0,0x3F,
        0x00,0x00,0x00,0x01,0xC0,0x3F,0x00,0x00,0x00,0x03,0xC0,0x1F,0x80,0x00,0x00,0x03,
        0x80,0x1F,0x80,0x00,0x00,0x07,0x80,0x0F,0x80,0x00,0x00,0x07,0x00,0x0F,0xC0,0x00,
        0x00,0x0F,0x00,0x07,0xC0,0x00,0x00,0x0E,0x00,0x07,0xE0,0x00,0x00,0x1E,0x00,0x03,
        0xE0,0x00,0x00,0x1F,0xFF,0xFF,0xF0,0x00,0x00,0x38,0x00,0x03,0xF0,0x00,0x00,0x38,
        0x00,0x01,0xF8,0x00,0x00,0x70,0x00,0x01,0xF8,0x00,0x00,0x70,0x00,0x00,0xFC,0x00,
        0x00,0xE0,0x00,0x00,0xFC,0x00,0x00,0xE0,0x00,0x00,0x7E,0x00,0x01,0xE0,0x00,0x00,
        0x7E,0x00,0x01,0xC0,0x00,0x00,0x3F,0x00,0x03,0xC0,0x00,0x00,0x3F,0x00,0x03,0xC0,
        0x00,0x00,0x3F,0x80,0x07,0xC0,0x00,0x00,0x3F,0xC0,0x7F,0xFE,0x00,0x03,0xFF,0xFC,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    },
    {
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x7F,0xFC,0x3F,0xFE,0x0F,0xFE,0x0F,0xE0,0x0F,0xF0,0x01,0xF8,0x0F,0xC0,0x03,0xE0,
        0x00,0xE0,0x07,0xC0,0x03,0xE0,0x00,0xE0,0x07,0xC0,0x03,0xE0,0x00,0xC0,0x07,0xE0,
        0x03,0xF0,0x01,0xC0,0x03,0xE0,0x01,0xF0,0x01,0xC0,0x03,0xE0,0x01,0xF0,0x01,0x80,
        0x03,0xE0,0x01,0xF8,0x03,0x80,0x01,0xF0,0x03,0xF8,0x03,0x80,0x01,0xF0,0x03,0xF8,
        0x03,0x00,0x01,0xF0,0x07,0xF8,0x07,0x00,0x01,0xF8,0x07,0xFC,0x06,0x00,0x00,0xF8,
        0x06,0x7C,0x06,0x00,0x00,0xF8,0x0E,0x7C,0x0E,0x00,0x00,0xF8,0x0C,0x7E,0x0C,0x00,
        0x00,0x7C,0x1C,0x3E,0x0C,0x00,0x00,0x7C,0x1C,0x3E,0x1C,0x00,0x00,0x7C,0x18,0x3E,
        0x18,0x00,0x00,0x3E,0x38,0x1F,0x18,0x00,0x00,0x3E,0x30,0x1F,0x38,0x00,0x00,0x3E,
        0x70,0x1F,0x30,0x00,0x00,0x1E,0x70,0x1F,0xB0,0x00,0x00,0x1F,0xE0,0x0F,0xF0,0x00,
        0x00,0x1F,0xE0,0x0F,0xE0,0x00,0x00,0x1F,0xC0,0x0F,0xE0,0x00,0x00,0x0F,0xC0,0x07,
        0xE0,0x00,0x00,0x0F,0xC0,0x07,0xC0,0x00,0x00,0x0F,0x80,0x07,0xC0,0x00,0x00,0x07,
        0x80,0x03,0xC0,0x00,0x00,0x07,0x00,0x03,0x80,0x00,0x00,0x07,0x00,0x03,0x80,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    }
};

/**
 * @brief Draw a 48x48 large character (for 'V', 'A', 'W')
 * @param x     Top-left X coordinate
 * @param y     Top-left Y coordinate
 * @param c     Character to display ('V', 'A', 'W')
 * @param color Character color
 */
void LCD_DrawBigChar_48x48(uint16_t x, uint16_t y, char c, uint16_t color) 
{
    uint8_t index;
    if(c == 'V') index = 0;
    else if(c == 'A') index = 1;
    else if(c == 'W') index = 2;
    else return;
    
    const uint8_t *pData = BigFont_48x48[index];
    for(uint8_t j = 0; j < 48; j++) {
        for(uint8_t i = 0; i < 48; i++) {
            uint8_t byte = pData[j * 6 + (i / 8)];
            if(byte & (0x80 >> (i % 8))) {
                LCD_DrawPoint(x + i, y + j, color);
            }
        }
    }
}