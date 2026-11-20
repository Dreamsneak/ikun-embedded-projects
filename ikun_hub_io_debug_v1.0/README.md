# ikun_hub_io_debug_v1.0

USB HUB + IO 调试板，纯硬件项目，用于 USB SuperSpeed 信号完整性与供电链路验证。

## 用途

- 多口 USB HUB 扩展与下行口供电切换
- IO 调试：引出关键信号、电平指示与跳线，便于示波器/逻辑分析仪抓取
- 配合 USB 分析设备排查 `LFPS`、`U1/U2` 挂起、`PEND_TIMEOUT` 等链路问题

## 目录结构

```
ikun_hub_io_debug_v1.0/
└── hardware/
    └── ikun_HUB_IO_DEBUG.eprj2   立创EDA 专业版工程
```

## 使用

用 **LCEDA Pro（立创EDA 专业版）** 打开 `.eprj2`，可直接查看原理图、PCB 与 BOM，导出 Gerber 打样。
