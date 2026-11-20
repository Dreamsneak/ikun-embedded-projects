# iKun 嵌入式项目集

个人嵌入式软硬件项目集合，每个项目一个独立目录，涵盖 ESP32-S3 / 沁恒 CH 系列 MCU 的固件开发，以及基于 **立创EDA 专业版（LCEDA Pro）** 的原理图与 PCB 设计。

## 项目一览

| 项目 | 主控 | 一句话说明 | 类型 |
| --- | --- | --- | --- |
| [ikun_thermal_imaging](ikun_thermal_imaging/) | ESP32-S3 | MLX90640 红外热成像手持终端：实时热图、伪彩、电量显示、按键冻结 | 固件 + 硬件 |
| [ikun_camera_v1.0](ikun_camera_v1.0/) | ESP32-S3 | 摄像头图像采集工程 | 固件 + 硬件 |
| [iKun_Power_Analyze](iKun_Power_Analyze/) | CH32V20x | 功耗分析仪：下位机采样 + Qt 上位机曲线显示 | 固件 + 上位机 + 硬件 |
| [ikun_hub_io_debug_v1.0](ikun_hub_io_debug_v1.0/) | — | USB HUB + IO 调试板，用于 USB SS 信号与供电链路验证 | 硬件 |

## 目录约定

每个 ikun 系列项目统一采用下面的结构，方便快速定位：

```
<项目名>/
├── README.md       项目说明：功能、引脚、依赖、编译方式
├── firmware/       MCU 固件工程（ESP-IDF / MounRiver / CMake）
└── hardware/       立创EDA 原理图与 PCB（.eprj2）
```

带 Qt 上位机的项目额外包含 `host/` 目录。

## 通用说明

- 编译产物（`build/`、`obj/`）、压缩包与工具链备份已由 `.gitignore` 排除，克隆后需自行编译。
- `.eprj2` 为立创EDA 专业版工程主文件，用 **LCEDA Pro** 打开（非标准版）。
- 各项目的引脚定义、外设参数写在对应 README 与源码头部注释中，以源码为准。
