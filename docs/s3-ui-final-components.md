# S3 最终 UI 页面和组件清单

这份清单是固件已经支持的稳定绑定名称。你在 SquareLine 里按这些对象名创建页面和组件后，重新导出即可；未创建的组件会被固件自动跳过，不影响编译和主驾驶页运行。

## 页面

| 页面 | 建议 Screen 名 | 用途 |
| --- | --- | --- |
| 主驾驶页 | `ui_ScreenMain` 或继续使用当前主屏 | 行驶状态、速度、电量、连接、油门、指南针、BMP280、MCU 温度 |
| 诊断页 | `ui_ScreenDiag` | 链路质量、协议版本、包序号、接收端状态、VESC、传感器状态 |
| 校准页 | `ui_ScreenCal` | 摇杆中心值、ADC 原始值、输出值、档位 ADC、校准按钮 |
| 系统页 | `ui_ScreenSystem` | 固件版本、背光亮度、电源模式、升级提示 |

## 主驾驶页

当前固件已经绑定以下对象。保留这些名称即可继续工作。

| 对象名 | 类型 | 固件行为 |
| --- | --- | --- |
| `ui_LabelStatus` | Label | `OK`、`LOST`、`STBY`、`TAKEOVER`、`RX FS`、`PROTO`、`OK LOCK` |
| `ui_LabelStatusVoltage` | Label | 接收端 VESC 电压 |
| `ui_ArcStatusVoltage` | Arc | 接收端 VESC 电压进度 |
| `ui_LabelBattery` | Label | S3 本机电量百分比 |
| `ui_ArcBattery` | Arc | S3 本机电量进度 |
| `ui_LabelSpeed` | Label | 接收端速度 |
| `ui_LabelControl` | Label | 档位 |
| `ui_BarThrottle` | Bar | 油门/刹车输出 |
| `ui_LabelBmp280` | Label | BMP280 气压和海拔 |
| `ui_Compass` | Image | QMC5883L 航向，按角度实时旋转 |
| `ui_MCUTemp` | Label | ESP32-S3 内部温度，格式如 `43.2C` |

## 诊断页

| 对象名 | 类型 | 显示格式 |
| --- | --- | --- |
| `ui_LabelLinkDiag` | Label | `RSSI -62 PKT 48 LOSS 3` |
| `ui_LabelRxFlags` | Label | `OK` / `FS PROTO` / `VESC LOCK` |
| `ui_LabelProtocol` | Label | `CTRL v2 STAT v2` |
| `ui_LabelSeq` | Label | `TX 12 RX 34` |
| `ui_LabelVesc` | Label | `VESC OK` / `VESC N/A` |
| `ui_LabelSensors` | Label | `CW OK BMP OK QMC OK` |

## 校准页

| 对象名 | 类型 | 固件行为 |
| --- | --- | --- |
| `ui_LabelJoyCenter` | Label | 当前持久化摇杆中心值 |
| `ui_LabelJoyRaw` | Label | 摇杆 ADC 原始值 |
| `ui_LabelJoyOut` | Label | 当前油门输出值 |
| `ui_LabelJoyRange` | Label | `MIN 0 MAX 4095 DZ 50` |
| `ui_LabelSpeedAdc` | Label | 档位电阻分压 ADC 原始值 |
| `ui_ButtonJoyCal` | Button | 重新采样并保存摇杆中心 |
| `ui_ButtonJoyMinus` | Button | 中心值 -10 并保存 |
| `ui_ButtonJoyPlus` | Button | 中心值 +10 并保存 |
| `ui_ButtonJoyReset` | Button | 恢复默认校准并保存 |

## 系统页

| 对象名 | 类型 | 固件行为 |
| --- | --- | --- |
| `ui_LabelFirmware` | Label | 固件标识和构建日期 |
| `ui_LabelBrightness` | Label | 当前背光值，格式如 `BRI 140` |
| `ui_SliderBrightness` | Slider | 手动设置背光亮度，范围 `20..255`，拖动后立即生效 |
| `ui_LabelPowerMode` | Label | `ACTIVE` / `DIM` / `STANDBY` |
| `ui_LabelUpgrade` | Label | USB 刷机提示：`USB: pio upload` |

## 绑定规则

- 不要在 `src/transmitter_s3/ui/generated/` 手写业务逻辑；该目录只放 SquareLine 导出文件。
- 新增组件必须使用上表对象名。对象不存在时固件会跳过绑定，存在后会自动显示数据或响应按钮。
- 页面跳转动画和布局由 SquareLine 处理；固件只负责数据、按钮动作、亮度滑条和安全状态。
- 如果你改了对象名，需要同步改 `src/transmitter_s3/ui/ui.cpp`，否则固件不会找到组件。
