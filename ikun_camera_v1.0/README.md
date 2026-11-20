# ikun_camera_v1.0

基于 **ESP32-S3** 的摄像头采集工程，使用 `esp32-camera` 组件驱动 OV 系列摄像头，采集图像后经 ILI9341 LCD 实时预览。

## 功能

- 摄像头 SCCB 配置 + DVP 并口数据采集（OV5640 / OV2640，按 `main.c` 中的引脚宏切换）
- 帧数据直接送 ILI9341 屏显示（RGB565）
- 开机画面使用内置 `ikun_image.h` 位图

## 摄像头引脚（ESP32-S3）

| 信号 | GPIO | 信号 | GPIO |
| --- | --- | --- | --- |
| RESET | 46 | D7 | 17 |
| XCLK | 3 | D6 | 16 |
| SIOD (SDA) | 41 | D5 | 15 |
| SIOC (SCL) | 42 | D4 | 20 |
| VSYNC | 8 | D3 | 7 |
| HREF | 19 | D2 / D1 / D0 | 6 / 5 / 4 |

> `D4` 在原理图上未标注，如出现图像错位优先排查该引脚。

## 目录结构

```
ikun_camera_v1.0/
├── firmware/
│   ├── main/
│   │   ├── main.c             摄像头初始化与预览主循环
│   │   ├── lcd_ili9341.c      LCD 驱动
│   │   └── ikun_image.h       开机位图
│   ├── components/esp32-camera
│   └── sdkconfig
└── hardware/
    └── iKun_Camera_V1.0.eprj2 原理图 / PCB
```

## 编译

```bash
cd firmware
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```
