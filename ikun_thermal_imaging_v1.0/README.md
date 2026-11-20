# ikun_thermal_imaging_v1.0

基于 **ESP32-S3 + MLX90640** 的红外热成像手持终端。通过 I2C 读取 32×24 红外测温阵列，转换为伪彩色热图后显示在 ILI9341 LCD 上，并叠加电池电量指示。

## 功能

- **实时热成像**：MLX90640 帧率 16 Hz，伪彩映射后按 10 倍放大显示（32×24 → 320×240）
- **伪彩色标定**：按帧内最低/最高温度动态映射色阶
- **电量显示**：分压电阻采样（R12=5.1K / R11=10K）计算电池电压与百分比，右上角绘制电池图标
- **画面冻结**：按键（IO47）切换显示暂停，冻结当前帧，后台采样不中断
- **屏幕帧缓存**：RGB565 帧缓存镜像面板内容，便于截图/二次处理

## 硬件接口

| 外设 | 接口 | 引脚 / 参数 |
| --- | --- | --- |
| MLX90640 | I2C0 | SCL = GPIO42，SDA = GPIO41，1 MHz（FastMode+） |
| ILI9341 LCD | SPI | 240×320，RGB565 |
| 电池检测 | ADC1_CH3 | GPIO4，分压比 R12/R11 |
| 冻结按键 | GPIO | IO47 |
| SD 卡 | SPI | 见 `main/sd_card.c` |

> MLX90640 读取一帧约 1.8 KB，必须跑满 1 MHz 才能塞进 16 Hz 的 62.5 ms 预算；若模块接线较长出现 I2C 错误，可降到 400 kHz 并把帧率改为 8 Hz。

## 目录结构

```
ikun_thermal_imaging_v1.0/
├── firmware/                  ESP-IDF 工程（target: esp32s3）
│   ├── main/
│   │   ├── main.c             任务调度与 I2C/按键初始化
│   │   ├── mlx90640.c        MLX90640 驱动（帧读取、参数校准）
│   │   ├── MLX90640_API.c    Melexis 官方 API
│   │   ├── thermal_display.c 温度→RGB565 伪彩映射
│   │   ├── lcd_ili9341.c     LCD 驱动
│   │   ├── battery_adc.c     电池电压采样
│   │   └── sd_card.c         SD 卡读写
│   ├── CMakeLists.txt
│   └── sdkconfig
└── hardware/
    ├── iKun_Thermal_Imaging.eprj2   主板原理图/PCB
    └── 24RGBLCD.eprj2               LCD 转接板
```

## 编译与烧录

```bash
cd firmware
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

依赖 ESP-IDF v5.x，使用标准 `i2c`、`spi`、`adc` 驱动，无需额外组件。
