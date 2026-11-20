#ifndef __CURVE_H
#define __CURVE_H

#include "lcd.h"

typedef struct {
    uint16_t x, y;          // 曲线区域左上角坐标
    uint16_t w, h;          // 宽高
    int16_t y_min, y_max;   // 数据范围
    uint16_t line_color;    // 曲线颜色
    uint16_t bg_color;      // 背景色
    uint16_t grid_color;    // 网格颜色
    uint8_t draw_grid;      // 是否显示网格
    uint16_t *buf;          // 数据缓冲区，大小为 w，存储每个X点对应的Y坐标(0~h-1)
    uint16_t count;         // 当前已填充的数据点数量
} CurveArea;

void Curve_Init(CurveArea *area, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                int16_t y_min, int16_t y_max, uint16_t line_color, uint16_t bg_color);
void Curve_AddPoint(CurveArea *area, int16_t data);
void Curve_Clear(CurveArea *area);
void Curve_Deinit(CurveArea *area);

#endif