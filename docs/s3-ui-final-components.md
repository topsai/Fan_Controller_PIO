# S3 最终 UI 页面和组件清单

以下是完成本轮所有固件能力后，建议在 SquareLine 中补齐的页面和对象名。添加并导出后，固件适配层即可按这些稳定名称绑定数据。

## 页面

| 页面 | 建议 Screen 名 | 用途 |
| --- | --- | --- |
| 主驾驶页 | `ui_ScreenMain` 或继续使用 `ui_Screen1` | 行驶状态、速度、电量、连接、油门、指南针 |
| 诊断页 | `ui_ScreenDiag` | 链路、协议、VESC、传感器、包速率 |
| 校准页 | `ui_ScreenCal` | 摇杆中心、微调、保存状态、恢复默认 |
| 系统页 | `ui_ScreenSystem` | 固件版本、亮度、电源、温度、升级提示 |

## 主驾驶页已有或建议保留

| 对象名 | 类型 | 数据 |
| --- | --- | --- |
| `ui_LabelStatus` | Label | `OK`、`LOST`、`RX FS`、`PROTO`、`LOCK` |
| `ui_LabelStatusVoltage` | Label | 接收端 VESC 电压 |
| `ui_ArcStatusVoltage` | Arc | 接收端 VESC 电压进度 |
| `ui_LabelBattery` | Label | S3 本机 SOC |
| `ui_ArcBattery` | Arc | S3 本机 SOC |
| `ui_LabelSpeed` | Label | 接收端速度 |
| `ui_LabelControl` | Label | 档位 |
| `ui_BarThrottle` | Bar | throttle/brake |
| `ui_LabelBmp280` | Label | BMP280 气压/海拔 |
| `ui_Compass` | Image | QMC5883L 航向 |
| `ui_MCUTemp` | Label | ESP32-S3 内部温度 |

## 诊断页新增

| 对象名 | 类型 | 显示格式 |
| --- | --- | --- |
| `ui_LabelLinkDiag` | Label | `RSSI -62 PKT 48 LOSS 3` |
| `ui_LabelRxFlags` | Label | `OK` / `FS PROTO` / `VESC LOCK` |
| `ui_LabelProtocol` | Label | `CTRL v2 STAT v2` |
| `ui_LabelSeq` | Label | 控制包序号、状态包序号 |
| `ui_LabelVesc` | Label | `VESC OK` / `VESC N/A` |
| `ui_LabelSensors` | Label | `CW/BMP/QMC` 有效状态 |

## 校准页新增

| 对象名 | 类型 | 用途 |
| --- | --- | --- |
| `ui_LabelJoyCenter` | Label | 当前持久化中心值 |
| `ui_LabelJoyRaw` | Label | 当前 ADC 原始值 |
| `ui_LabelJoyOut` | Label | 当前 throttle 输出 |
| `ui_ButtonJoyCal` | Button | 重新采样并保存中心 |
| `ui_ButtonJoyMinus` | Button | 中心值 -10 并保存 |
| `ui_ButtonJoyPlus` | Button | 中心值 +10 并保存 |
| `ui_ButtonJoyReset` | Button | 恢复默认校准 |

## 系统页新增

| 对象名 | 类型 | 用途 |
| --- | --- | --- |
| `ui_LabelFirmware` | Label | 固件版本和构建日期 |
| `ui_LabelBrightness` | Label | 当前背光值 |
| `ui_SliderBrightness` | Slider | 手动亮度设置 |
| `ui_LabelPowerMode` | Label | `ACTIVE` / `DIM` |
| `ui_LabelUpgrade` | Label | USB 刷机提示 |

## 绑定原则

- 不在 `src/transmitter_s3/ui/generated/` 手写业务逻辑。
- 新增对象名必须稳定，导出后在 `src/transmitter_s3/ui/ui.cpp` 增加访问器和空指针保护。
- 没有添加组件前，固件只保留状态字段和格式 helper，不引用未生成的对象名，保证编译可用。
