# S3 UI 最终页面和组件

这份文档记录固件已经支持的 S3 SquareLine 页面、组件名和中文小字库要求。SquareLine 工程内对象名不要带 `ui_`；导出后固件看到的符号会自动带 `ui_` 前缀。

本轮只维护 SquareLine 工程源文件，不手改 `src/transmitter_s3/ui/generated/`。你在 SquareLine 中打开工程、调整展示效果、重新导出后，再编译 S3 固件。

## 页面

| 页面 | SquareLine Screen 名 | 导出符号 | 用途 |
| --- | --- | --- | --- |
| 主驾驶页 | `ScreenMain` | `ui_ScreenMain` | 行驶状态、速度、电量、连接、油门、指南针、BMP280、MCU 温度 |
| 诊断页 | `ScreenDiag` | `ui_ScreenDiag` | 链路质量、协议版本、包序号、接收端状态、VESC、传感器状态 |
| 校准页 | `ScreenCal` | `ui_ScreenCal` | 摇杆中心、ADC 原始值、输出值、范围、速度旋钮、校准按钮 |
| 系统页 | `ScreenSystem` | `ui_ScreenSystem` | 固件版本、背光亮度、电源模式、升级提示 |

## 主驾驶页

| 导出对象 | 类型 | 固件行为 |
| --- | --- | --- |
| `ui_LabelStatus` | Label | 中文连接/锁定/接管/故障状态 |
| `ui_LabelStatusVoltage` | Label | 接收端 VESC 电压 |
| `ui_ArcStatusVoltage` | Arc | 接收端 VESC 电压进度 |
| `ui_LabelBattery` | Label | S3 本机电量百分比 |
| `ui_ArcBattery` | Arc | S3 本机电量进度 |
| `ui_LabelSpeed` | Label | 接收端速度 |
| `ui_LabelControl` | Label | 档位 |
| `ui_BarThrottle` | Bar | 油门/刹车输出 |
| `ui_LabelBmp280` | Label | BMP280 气压和高度 |
| `ui_Compass` | Image | QMC5883L 航向，按角度实时旋转 |
| `ui_MCUTemp` | Label | ESP32-S3 内部温度，格式如 `43.2C` |

## 诊断页

| 导出对象 | 类型 | 显示示例 |
| --- | --- | --- |
| `ui_LabelLinkDiag` | Label | `信号 -62 包 48 丢 3` |
| `ui_LabelRxFlags` | Label | `正常` / `失效 协议` / `电调 锁定` |
| `ui_LabelProtocol` | Label | `协议 控制v2 状态v2` |
| `ui_LabelSeq` | Label | `发送 12 接收 34` |
| `ui_LabelVesc` | Label | `电调 正常` / `电调 无数据` |
| `ui_LabelSensors` | Label | `传感器 电池正常 气压无 指南正常` |

## 校准页

| 导出对象 | 类型 | 显示/行为 |
| --- | --- | --- |
| `ui_LabelJoyCenter` | Label | `摇杆中心 2048` |
| `ui_LabelJoyRaw` | Label | `摇杆原始 2048` |
| `ui_LabelJoyOut` | Label | `摇杆输出 0` |
| `ui_LabelJoyRange` | Label | `最小 0 最大 4095 死区 50` |
| `ui_LabelSpeedAdc` | Label | `速度旋钮 0` |
| `ui_ButtonJoyMinus` | Button | 中心值减 10 |
| `ui_ButtonJoyCal` | Button | 采样当前摇杆中心 |
| `ui_ButtonJoyPlus` | Button | 中心值加 10 |
| `ui_ButtonJoyReset` | Button | 重置校准 |

## 系统页

| 导出对象 | 类型 | 显示/行为 |
| --- | --- | --- |
| `ui_LabelFirmware` | Label | 固件版本和构建日期 |
| `ui_LabelBrightness` | Label | 当前背光亮度 |
| `ui_SliderBrightness` | Slider | 用户背光亮度，范围 20-255 |
| `ui_LabelPowerMode` | Label | `活动` / `变暗` / `待机` |
| `ui_LabelUpgrade` | Label | `升级 USB烧录` |

## 中文小字库

固定中文文案使用 `ChineseSmall` 小字库。文件位于：

- `tools/ui_projects/s3_transmitter_squareline/assets/fonts/ui_font_ChineseSmall.fcfg`
- `tools/ui_projects/s3_transmitter_squareline/assets/fonts/ui_font_ChineseSmall.bin`
- `tools/ui_projects/s3_transmitter_squareline/assets/fonts/ui_font_ChineseSmall.c`
- `tools/ui_projects/s3_transmitter_squareline/assets/fonts/s3_chinese_small_symbols.txt`

该字库只覆盖 S3 UI 固定中文和 ASCII，不是完整中文字库。新增中文文案时，先把新增汉字加入 `s3_chinese_small_symbols.txt`，再重新生成 `ui_font_ChineseSmall.bin` 和 `ui_font_ChineseSmall.c`。
