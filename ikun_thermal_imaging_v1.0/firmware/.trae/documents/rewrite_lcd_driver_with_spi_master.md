# 重写 LCD 驱动:用 spi_master 直接驱动 ILI9341(照搬 CH32 验证代码)

## Context(为什么做这个改动)

ESP32 上 ILI9341 颜色一直不对,反复调整 swap_gb 方向、gamma 寄存器、通道增益均未能解决。根因有两个:

1. **MADCTL 寄存器值不对**:CH32 验证过的代码 [lcd.c:326](file:///D:/Research%20materials/ESP32_IDF/ikun_thermal_imaging/main/lcd.c#L326) 用 `MADCTL=0x68`(BGR=1, MX=1, MV=1),BGR 位让 ILI9341 硬件自动匹配面板的子像素物理排列(B-G-R)。当前 ESP32 代码用 `MADCTL=0x20`(BGR=0),导致 R/B 子像素映射错误,被迫用软件 swap_gb 去"猜"交换方向。
2. **esp_lcd 抽象层的字节序不可控**:CH32 代码 [lcd.c:102-112](file:///D:/Research%20materials/ESP32_IDF/ikun_thermal_imaging/main/lcd.c#L102-L112) 手动发高字节再发低字节,字节序确定。ESP32 用 `esp_lcd_panel_io_tx_color` 发送 16-bit buffer,小端架构下字节顺序不可控,导致颜色错乱。memory 里已记录教训:"esp_lcd abstract layer caused display issues; direct SPI transaction with manual DC control is more reliable"。

**目标**:完全复刻 CH32 上"颜色 OK"的驱动逻辑到 ESP32,用 `driver/spi_master.h` 直接控制 SPI + GPIO,放弃 esp_lcd 抽象层。参考文件 `spi.c`/`lcd.c`/`lcd.h` 不动(仅参考)。

## 关键技术决策

- **MADCTL = 0x68**(照搬 CH32 `LCD_direction(USE_HORIZONTAL)` 的方向1),BGR=1 让硬件处理子像素映射,不需要任何软件通道交换
- **swap_gb 改成 no-op**(保留函数签名避免编译错误,`thermal_display.c` 依赖它),因为 BGR=1 已在硬件层解决
- **16-bit 数据字节序**:ESP32 小端,用 spi_master 发送时按内存顺序逐字节发。为匹配 CH32"高字节先发",在构建发送 buffer 时把每个像素存为大端格式(`(c>>8) | (c<<8)`)
- **SPI 速度 40MHz**,Mode 0,MSB first(与 CH32 一致)。若花屏再降速,但预期 spi_master 直接控制能稳定跑 40MHz(之前问题是 esp_lcd 的 queue_depth 事务管理,不是速度本身)
- **公共接口完全不变**:`lcd_init`/`fill_screen`/`fill_rect`/`lcd_draw_bitmap`/`lcd_draw_image2lcd`/`swap_gb` 签名保持,`hello_world_main.c` 和 `thermal_display.c` 不需要改

## 实施步骤

### 步骤 1:修复 CMakeLists.txt(移除 CH32 参考文件)
**文件**: [main/CMakeLists.txt](file:///d:/Research%20materials/ESP32_IDF/ikun_thermal_imaging/main/CMakeLists.txt)

从 SRCS 移除 `"lcd.c"` 和 `"spi.c"`(它们是 CH32 参考,依赖不存在的 `debug.h`/`config.h`/`font.h`,不应参与编译)。同时从 REQUIRES 移除 `esp_lcd`(不再使用)。保留 `driver`(spi_master/gpio)。

### 步骤 2:重写 lcd_ili9341.h(简化,移除 esp_lcd 依赖)
**文件**: [main/lcd_ili9341.h](file:///d:/Research%20materials/ESP32_IDF/ikun_thermal_imaging/main/lcd_ili9341.h)

- 保持引脚定义不变(SCLK=36, MOSI=35, CS=38, DC=39, RST=40, BCKL=37)
- 保持 LCD_WIDTH=320, LCD_HEIGHT=240
- `swap_gb()` 改成 no-op:`return c;`(BGR=1 已处理,保留签名供 thermal_display.c 调用)
- 移除 `LCD_RED_GAIN`/`LCD_GREEN_GAIN`/`LCD_BLUE_GAIN` 宏(不再需要软件增益)
- 移除 `IMAGE2LCD_MSB_FIRST` 宏的复杂说明,保留默认值 1
- 保持所有公共函数声明不变
- include 改为 `<stdint.h>` + `"esp_err.h"`,移除 esp_lcd 头文件

### 步骤 3:重写 lcd_ili9341.c(核心,用 spi_master 直接驱动)
**文件**: [main/lcd_ili9341.c](file:///d:/Research%20materials/ESP32_IDF/ikun_thermal_imaging/main/lcd_ili9341.c)

完全重写,结构如下:

**底层 SPI + GPIO 控制**(照搬 CH32 lcd.c 的函数语义):
- `lcd_spi_init()`:用 `spi_bus_initialize` + `spi_bus_add_device` 初始化 SPI2,Mode 0,40MHz,MSB first
- `lcd_gpio_init()`:配置 DC/CS/RST/BCKL 为输出,初始状态 CS=高(空闲)、DC=高(数据)、BCKL=高(开背光)
- `lcd_reset()`:RST 拉低 100ms → 拉高 50ms(照搬 CH32 `LCD_RESET`)
- `lcd_wr_cmd(uint8_t cmd)`:CS=低, DC=低(命令), spi 发 1 字节, CS=高
- `lcd_wr_data(uint8_t data)`:CS=低, DC=高(数据), spi 发 1 字节, CS=高
- `lcd_wr_data16(uint16_t data)`:CS=低, DC=高, spi 发高字节再发低字节(照搬 CH32 `Lcd_WriteData_16Bit`)
- `lcd_set_window(x1,y1,x2,y2)`:发 0x2A(列地址) + 0x2B(行地址) + 0x2C(写内存)(照搬 CH32 `LCD_SetWindows`)

**lcd_init()**(照搬 CH32 `LCD_Init` 的完整寄存器序列):
- 调用 spi_init + gpio_init + reset + 开背光
- 完整 init 序列(0xCF/0xED/0xE8/0xCB/0xF7/0xEA/0xC0/0xC1/0xC5/0xC7/0x3A/0xB1/0xB6/0xF2/0x26/0xE0/0xE1/0x2B/0x2A),值与 CH32 lcd.c:180-266 完全一致
- 0x11 退出睡眠,延迟 120ms
- MADCTL = 0x68(照搬 CH32 方向1)
- 0x29 开显示

**fill_screen(color)**:
- lcd_set_window(0, 0, 319, 239)
- 逐行发送,每行构建大端格式 buffer(每像素 `(color>>8)|(color<<8)`),用 spi_device_polling_transmit 批量发送

**fill_rect(x1,y1,x2,y2,color)**:
- lcd_set_window(x1, y1, x2-1, y2-1)
- 逐行发送大端格式 buffer

**lcd_draw_bitmap(x1,y1,x2,y2,color_data)**:
- lcd_set_window(x1, y1, x2-1, y2-1)
- 申请临时发送 buffer(大小 = 像素数 * 2 字节)
- 遍历输入 uint16_t color_data,逐像素转大端填入发送 buffer
- spi_device_polling_transmit 一次性发送整块
- 释放临时 buffer
- 注意:输入 color_data 是标准 RGB565,不需要 swap_gb(BGR=1 硬件处理)

**lcd_draw_image2lcd(x,y,image_data,w,h)**:
- 保持现有的 8 字节头解析 + 垂直扫描转水平逻辑
- 移除 swap_gb 调用(BGR=1 不需要)
- 16-bit 像素按大端格式填入发送 buffer

### 步骤 4:thermal_display.c 微调(可选,仅注释)
**文件**: [main/thermal_display.c](file:///d:/Research%20materials/ESP32_IDF/ikun_thermal_imaging/main/thermal_display.c)

- swap_gb 调用保留(已是 no-op,不影响逻辑)
- 注释更新:说明 BGR=1 硬件处理通道映射,swap_gb 为 no-op

## 不需要改的文件
- `hello_world_main.c`(接口不变,8 色测试逻辑不变)
- `spi.c`/`lcd.c`/`lcd.h`(用户要求不动,仅参考)
- `battery_adc.c/h`、`mlx90641.c/h`、`thermal_display.h`、`ikun_image.h`

## 验证方法

1. `idf.py fullclean && idf.py build`(必须 fullclean,避免旧对象文件缓存)
2. `idf.py -p COM98 flash monitor`
3. 观察 8 色测试(红绿蓝黄青品红白黑),对照 CH32 代码的颜色定义:
   - 红 0xF800、绿 0x07E0、蓝 0x001F、黄 0xFFE0、青 0x07FF、品红 0xF81F、白 0xFFFF、黑 0x0000
   - 预期:全部显示正确(因为完全复刻 CH32 验证代码)
4. 验证 ikun 图像颜色
5. 若 8 色正确但图像偏色,检查 IMAGE2LCD_MSB_FIRST 和图像字节序(独立问题)
