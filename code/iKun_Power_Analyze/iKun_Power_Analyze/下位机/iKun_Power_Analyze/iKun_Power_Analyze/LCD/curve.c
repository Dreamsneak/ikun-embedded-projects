/*********************************************************************
 * File Name          : curve.c
 * Author             : 混分巨兽龙某某
 * Version            : V1.0.0
 * Date               : 2026/4/29
 * Description        : Curve drawing driver for LCD, implement real-time waveform display
*******************************************************************************/

#include "curve.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief   Fast fill rectangle by LCD burst write
 * @param   x:      Rectangle start X coordinate
 * @param   y:      Rectangle start Y coordinate
 * @param   w:      Rectangle width
 * @param   h:      Rectangle height
 * @param   color:  Fill color (RGB565)
 * @retval  None
 */
static void FillRectFast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    LCD_SetWindows(x, y, x + w - 1, y + h - 1);
    LCD_CS_CLR;
    LCD_DC_SET;
    for (uint32_t i = 0; i < (uint32_t)w * h; i++) {
        SPI2_Write(color >> 8);
        SPI2_Write(color & 0xFF);
    }
    LCD_CS_SET;
    LCD_SetWindows(0, 0, LCD_H - 1, LCD_W - 1);
}

/**
 * @brief   Draw coordinate grid for curve area
 * @param   area:   Pointer to curve display area structure
 * @retval  None
 * @note    Draw left border, bottom border, zero line and vertical dashed grid
 */
static void DrawGrid(CurveArea *area) {
    if (!area->draw_grid) return;
    uint16_t x0 = area->x, y0 = area->y, w = area->w, h = area->h;
    
    /* Draw bottom border */
    for (uint16_t i = 0; i < w; i++) {
        LCD_DrawPoint(x0 + i, y0 + h - 1, area->grid_color);
    }
    /* Draw left border */
    for (uint16_t j = 0; j < h; j++) {
        LCD_DrawPoint(x0, y0 + j, area->grid_color);
    }
    
    /* Draw horizontal zero reference line */
    if (area->y_min <= 0 && area->y_max >= 0) {
        uint16_t yzero = y0 + (uint16_t)((long)(h - 1) * (0 - area->y_min) / (area->y_max - area->y_min));
        if (yzero != y0) {
            for (uint16_t i = 0; i < w; i++) {
                LCD_DrawPoint(x0 + i, yzero, area->grid_color);
            }
        }
    }
    
    /* Draw vertical dashed grid line (interval 20 pixels) */
    for (uint16_t x = x0 + 20; x < x0 + w - 1; x += 20) {
        for (uint16_t y = y0 + 1; y < y0 + h - 1; y += 5) {
            LCD_DrawPoint(x, y, area->grid_color);
        }
    }
}

/**
 * @brief   Convert raw data value to LCD Y pixel coordinate
 * @param   area:   Pointer to curve display area structure
 * @param   data:   Raw input data
 * @retval  Calculated Y offset inside curve area
 * @note    Limit data in [y_min, y_max], adapt LCD top-down coordinate system
 */
static uint16_t DataToY(CurveArea *area, int16_t data) {
    /* Clamp data to valid range */
    if (data < area->y_min) data = area->y_min;
    if (data > area->y_max) data = area->y_max;

    /* Convert data to Y pixel (LCD Y increases downward) */
    uint16_t y_pixel = area->h - 
        ((int32_t)(data - area->y_min) * area->h) / (area->y_max - area->y_min);

    return y_pixel;
}

/**
 * @brief   Initialize curve display area and buffer
 * @param   area:       Pointer to CurveArea struct
 * @param   x:          Area start X
 * @param   y:          Area start Y
 * @param   w:          Area width
 * @param   h:          Area height
 * @param   y_min:      Minimum Y data range
 * @param   y_max:      Maximum Y data range
 * @param   line_color: Curve line color
 * @param   bg_color:   Background color
 * @retval  None
 */
void Curve_Init(CurveArea *area, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                int16_t y_min, int16_t y_max, uint16_t line_color, uint16_t bg_color) {
    area->x = x; area->y = y; area->w = w; area->h = h;
    area->y_min = y_min; area->y_max = y_max;
    area->line_color = line_color; area->bg_color = bg_color;
    area->grid_color = LCD_GRAY;
    area->draw_grid = 1;
    area->buf = (uint16_t*)malloc(w * sizeof(uint16_t));
    area->count = 0;  /* Reset data point counter */
    
    if (area->buf) {
        for (uint16_t i = 0; i < w; i++) {
            area->buf[i] = 0xFFFF;
        }
    }
    
    FillRectFast(x, y, w, h, bg_color);
    DrawGrid(area);
}

/**
 * @brief   Add one new data point and draw curve line
 * @param   area:   Pointer to CurveArea struct
 * @param   data:   New input data value
 * @retval  None
 * @note    Auto clear & restart when buffer is full
 */
void Curve_AddPoint(CurveArea *area, int16_t data) {
    if (!area->buf) return;
    
    uint16_t new_y = DataToY(area, data);
    
    if (area->count < area->w) {
        uint16_t current_x = area->x + area->count;
        area->buf[area->count] = new_y;
        
        /* Draw line between current point and previous point */
        if (area->count > 0 && area->buf[area->count - 1] != 0xFFFF) {
            LCD_DrawLine(current_x - 1, area->y + area->buf[area->count - 1],
                         current_x, area->y + new_y,
                         area->line_color);
        }
        
        area->count++;
    } else {
        /* Buffer full, clear all and restart drawing */
        Curve_Clear(area);
        area->buf[0] = new_y;
        area->count = 1;
    }
}

/**
 * @brief   Clear curve area and reset data buffer
 * @param   area:   Pointer to CurveArea struct
 * @retval  None
 */
void Curve_Clear(CurveArea *area) {
    if (!area->buf) return;
    for (uint16_t i = 0; i < area->w; i++) {
        area->buf[i] = 0xFFFF;
    }
    area->count = 0;
    FillRectFast(area->x, area->y, area->w, area->h, area->bg_color);
    DrawGrid(area);
}

/**
 * @brief   Deinitialize curve module, release dynamic memory
 * @param   area:   Pointer to CurveArea struct
 * @retval  None
 */
void Curve_Deinit(CurveArea *area) {
    if (area->buf) {
        free(area->buf);
        area->buf = NULL;
    }
}