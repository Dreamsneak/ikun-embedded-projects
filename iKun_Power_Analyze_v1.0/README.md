# iKun_Power_Analyze_v1.0

功耗分析仪完整方案：下位机实时采样电压/电流，上位机绘制功率曲线。

## 组成

```
iKun_Power_Analyze_v1.0/
├── firmware/   CH32V20x 下位机固件（MounRiver Studio / GCC 工程）
├── host/       Qt 上位机（QWidget + 自绘图表 chartwidget）
└── hardware/   立创EDA 工程与 Gerber
```

## 下位机（firmware/）

- 主控 **CH32V20x**（RISC-V），工程文件为 `.wvproj` / `.cproject`
- INA226 电流/电压监测芯片，经 I2C 读取母线电压、分流电压、功率
- 本地 24 寸 RGB LCD 显示实时曲线（`LCD/curve.c` 自绘）
- 通过串口（USB）把采样数据上报上位机

## 上位机（host/）

- Qt 5 工程（`.pro`），串口接收下位机数据，`chartwidget` 负责实时波形绘制
- 带自定义图标资源 `ikun_Power.ico`

构建：用 Qt Creator 打开 `host/iKun_Power_Analyze/iKun_Power_Analyze.pro` 直接编译（qmake）。

## 硬件（hardware/）

| 文件 | 说明 |
| --- | --- |
| `iKun_Power_Analyze.eprj2` | 主控板原理图 / PCB |
| `24RGBLCD.eprj2` | 24 寸 RGB LCD 转接板 |
| `Gerber_*.zip` | 已导出的 Gerber，可直接打样 |
