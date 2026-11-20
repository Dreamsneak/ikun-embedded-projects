# iKun Embedded & Hardware Projects

个人嵌入式开发与硬件设计工程集合，涵盖沁恒（WCH）CH 系列 MCU、ESP32-S3 以及立创EDA（LCEDA-Pro）硬件工程。

## 目录结构

```
code/
├── ikun_camera_v1.0/        ESP32-S3 摄像头工程（ESP-IDF / CMake）
├── CH585EVT_1/              沁恒 CH585 蓝牙 EVT 开发与验证工程
└── iKun_Power_Analyze/      双 MCU 功耗分析仪（下位机 + Qt 上位机）
hardware/
├── ikun_hub_io_debug_v1.0/  USB HUB IO 调试板 立创EDA 专业版工程
└── CH570Q_wireless_uart_debugger/  CH570Q 无线串口调试神器（第11届立创电赛）
```

## 各工程说明

### ikun_camera_v1.0
基于 ESP32-S3 的图像采集工程，使用 ESP-IDF 构建（`CMakeLists.txt` + `sdkconfig`）。
硬件部分同目录下附有 `iKun_Camera_V1.0.eprj2` 立创EDA原理图/PCB工程。

### CH585EVT_1
沁恒 CH585（BLE + USB）评估板官方 EVT 与自建验证代码，含 BLE HID/NFC、低功耗等例程。

### iKun_Power_Analyze
功耗分析仪完整方案：
- `下位机/` CH32V20x 固件（MounRiver / GCC 工程）
- `上位机/` Qt 上位机程序（含 qrc 资源）
- `硬件/` 立创EDA工程（主控板 + 24RGBLCD 转接板）

### ikun_hub_io_debug_v1.0
USB HUB + IO 调试板，立创EDA 专业版工程（`.eprj2`），用于 USB SS 信号与供电链路验证。

### CH570Q_wireless_uart_debugger
CH570Q 无线串口调试工具，第 11 届立创电子设计大赛参赛作品。

## 说明

- 编译产物（`build/`、`obj/`）、历史备份与压缩包已通过 `.gitignore` 排除。
- 立创EDA 工程使用 **LCEDA Pro（专业版）** 打开；`.eprj2` 为工程主文件。
- 代码仅供学习与交流使用。
