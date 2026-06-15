# UI 字段调试参考

本文档用于调试 UI、整理 SquareLine 组件、核对 C3 OLED 页面字段。它按“页面/组件/字段”列出对象名、显示格式、数据来源和调试要点。

## 1. 通用约定

| 项目 | 说明 |
| --- | --- |
| S3 UI 工具 | SquareLine Studio + LVGL |
| S3 生成代码目录 | `src/transmitter_s3/ui/generated/` |
| S3 业务绑定文件 | `src/transmitter_s3/ui/ui.cpp` |
| S3 状态结构 | `S3UiState`，定义于 `src/transmitter_s3/ui/ui.h` |
| C3 UI | 0.96 寸 SSD1306 OLED，直接在 `src/transmitter/main.cpp::updateDisplay()` 绘制 |
| C3 刷新率 | 约 10Hz |
| S3 当前实物环境 | `s3_transmitter` |
| S3 新 PCB 环境 | `s3_transmitter_new_pcb`，仅新板打板后使用 |

S3 的 SquareLine 对象如果尚未创建，固件会跳过对应绑定，不影响编译和主屏运行。对象创建后必须保持下表对象名，否则固件找不到组件。

## 2. S3 数据源字段

| `S3UiState` 字段 | 类型 | 来源 | 用途 |
| --- | --- | --- | --- |
| `connected` | `bool` | 是否收到接收端状态包且未超时 | 主状态、电压、速度显示有效性 |
| `receiverVoltageX100` | `uint16_t` | 接收端状态包 `voltage`，单位 V x100 | VESC/接收端电压 |
| `speedLevel` | `uint8_t` | 当前档位输入 | 档位显示 |
| `joystickValue` | `int16_t` | 安全处理后的油门/刹车输出，`-1000..1000` | 油门条、校准页输出 |
| `receiverSpeed` | `uint16_t` | 接收端状态包 `speed` | 速度显示 |
| `cw2015Valid` | `bool` | S3 本地 CW2015 读数可信状态 | 本机电量显示有效性 |
| `cw2015Soc` | `float` | S3 本地 CW2015 SOC | 本机电量百分比 |
| `bmp280Valid` | `bool` | BMP280 读数可信状态 | 气压/海拔显示有效性 |
| `bmp280PressureHpa` | `float` | BMP280 气压 | 气压显示 |
| `bmp280AltitudeM` | `float` | 气压估算海拔 | 海拔显示 |
| `qmcValid` | `bool` | QMC5883L 读数可信状态 | 指南针透明度 |
| `qmcHeadingDeg` | `float` | QMC5883L 航向角，单位度 | 指南针旋转角 |
| `mcuTemperatureC` | `float` | ESP32-S3 `temperatureRead()` | MCU 内部温度 |
| `mcuTemperatureWarning` | `bool` | 温度阈值判断 | MCU 温度颜色 |
| `rssiValue` | `int16_t` | 接收端状态包 RSSI 或诊断模拟值 | 诊断页链路质量 |
| `receiverStatusFlags` | `uint8_t` | 接收端状态 flags | 主状态、VESC、failsafe、协议异常 |
| `statusPacketRateHz` | `uint16_t` | S3 本地统计状态包速率 | 诊断页 |
| `statusLostPackets` | `uint16_t` | S3 本地估算状态包丢包 | 诊断页 |
| `controlSequence` | `uint16_t` | S3 控制包序号 | 诊断页 |
| `lastStatusSequence` | `uint16_t` | 最近接受的状态包序号 | 诊断页 |
| `displayBrightness` | `uint8_t` | 当前 LCD 背光值 | 系统页 |
| `displayDimmed` | `bool` | 是否处于降亮度模式 | 系统页 |
| `standbyMode` | `bool` | 未连接、未解锁、非设置、非接管状态 | 主状态、系统页 |
| `takeoverActive` | `bool` | 按钮1长按后的接管请求窗口 | 主状态 |
| `armed` | `bool` | 是否已刹车保持 3 秒解锁 | 主状态 |
| `settingsMode` | `bool` | 是否进入设置/校准模式 | 临时设置浮层 |
| `joystickCenter` | `int` | 当前持久化摇杆中心 | 校准页 |
| `joystickRawAdc` | `int` | 摇杆 ADC 原始值 | 校准页 |
| `joystickMinRaw` | `int` | 摇杆校准最小值 | 校准页 |
| `joystickMaxRaw` | `int` | 摇杆校准最大值 | 校准页 |
| `joystickDeadzone` | `int` | 摇杆死区 | 校准页 |
| `speedAdcRaw` | `uint16_t` | 新 PCB 档位 ADC 或旧板模拟值 | 校准页/档位调试 |
| `firmwareVersion` | `const char *` | 固件内置字符串 | 系统页 |
| `buildDate` | `const char *` | 编译日期 `__DATE__` | 系统页 |

## 3. S3 Screen 和组件

### 3.1 主驾驶页

当前 SquareLine 主屏名保持为 `Screen1`，导出后为 `ui_Screen1`。固件按对象名绑定主屏组件，不要求把主屏改名。

| 对象名 | 类型 | 显示格式/范围 | 数据来源 | 更新逻辑 | 调试要点 |
| --- | --- | --- | --- | --- | --- |
| `ui_LabelStatus` | Label | `OK`、`LOST`、`STBY`、`TAKEOVER`、`RX FS`、`PROTO`、`OK LOCK` | `connected`、`standbyMode`、`takeoverActive`、`armed`、`receiverStatusFlags` | `s3FormatMainStatusText()` | 颜色：故障红、接管青、待机灰、未解锁黄、正常绿 |
| `ui_LabelStatusVoltage` | Label | `48.00V` / `--.-V` | `receiverVoltageX100`、`connected` | 连接时显示电压，断联占位 | 对应接收端/VESC 电压，不是 S3 本机电池 |
| `ui_ArcStatusVoltage` | Arc | `6..48` | `receiverVoltageX100`、`connected` | `s3StatusVoltageForArc()` | 建议 Arc 范围保持 `6..48` |
| `ui_LabelBattery` | Label | `87%` / `--%` | `cw2015Valid`、`cw2015Soc` | `s3BatteryPercentForArc()` | 本机 S3 电量 |
| `ui_ArcBattery` | Arc | `0..100` | `cw2015Valid`、`cw2015Soc` | 低于等于 20% 红色，否则绿色 | 禁用可点击/拖动 |
| `ui_LabelSpeed` | Label | 整数速度，例如 `23` | `receiverSpeed` | 直接显示整数 | 颜色：低速绿，中速黄，高速红 |
| `ui_LabelControl` | Label | `1`、`2`、`3` | `speedLevel` | 直接显示档位 | 当前旧板为三路数字输入，新 PCB 为 ADC 分压 |
| `ui_BarThrottle` | Bar | `-1000..1000`，对称模式 | `joystickValue` | `LV_BAR_MODE_SYMMETRICAL` | 正值油门绿色，负值刹车橙色，零值灰色 |
| `ui_LabelBmp280` | Label | `1008hPa` + 换行 + `42m` / `BMP N/A` | `bmp280Valid`、`bmp280PressureHpa`、`bmp280AltitudeM` | BMP 无效时显示占位 | 海拔是估算值，不是精密校准 |
| `ui_Compass` | Image | 图片旋转角，LVGL 0.1 度单位 | `qmcValid`、`qmcHeadingDeg` | `lv_img_set_angle()` | 无效时透明度 50%，有效时不透明 |
| `ui_MCUTemp` | Label | `43.2C` / `--.-C` | `mcuTemperatureC`、`mcuTemperatureWarning` | `s3FormatMcuTemperatureText()` | 温度警告时红色，否则白色 |

### 3.2 诊断页

建议 Screen 名：`ui_ScreenDiag`。用于排查 ESP-NOW、协议包、状态 flags 和传感器状态。

| 对象名 | 类型 | 显示格式 | 数据来源 | 用途 |
| --- | --- | --- | --- | --- |
| `ui_LabelLinkDiag` | Label | `RSSI -62 PKT 48 LOSS 3` | `rssiValue`、`statusPacketRateHz`、`statusLostPackets` | 判断状态包速率和丢包 |
| `ui_LabelRxFlags` | Label | `OK` / `FS PROTO` / `VESC LOCK` | `receiverStatusFlags` | 解码接收端状态 |
| `ui_LabelProtocol` | Label | `CTRL v2 STAT v2` | `CONTROL_PROTOCOL_VERSION`、`STATUS_PROTOCOL_VERSION` | 确认协议版本 |
| `ui_LabelSeq` | Label | `TX 12 RX 34` | `controlSequence`、`lastStatusSequence` | 排查序号回绕/重启拒包 |
| `ui_LabelVesc` | Label | `VESC OK` / `VESC N/A` | `STATUS_FLAG_VESC_VALID` | 判断 VESC 数据是否有效 |
| `ui_LabelSensors` | Label | `CW OK BMP OK QMC OK` | `cw2015Valid`、`bmp280Valid`、`qmcValid` | 判断 S3 本地传感器有效性 |

### 3.3 校准页

建议 Screen 名：`ui_ScreenCal`。用于摇杆和档位调试。

| 对象名 | 类型 | 显示/动作 | 数据来源或动作 | 调试要点 |
| --- | --- | --- | --- | --- |
| `ui_LabelJoyCenter` | Label | `CENTER 2048` | `joystickCenter` | 当前持久化中心值 |
| `ui_LabelJoyRaw` | Label | `RAW 2048` | `joystickRawAdc` | 摇杆 ADC 原始值 |
| `ui_LabelJoyOut` | Label | `OUT 0` | `joystickValue` | 最终安全输出，不是原始 ADC |
| `ui_LabelJoyRange` | Label | `MIN 0 MAX 4095 DZ 50` | `joystickMinRaw`、`joystickMaxRaw`、`joystickDeadzone` | 校准范围和死区 |
| `ui_LabelSpeedAdc` | Label | `SPD ADC 2048` | `speedAdcRaw` | 新 PCB 用于调档位 ADC 阈值 |
| `ui_ButtonJoyCal` | Button | 重新采样并保存中心 | 返回 `CalibrateCenter` | 校准后需要重新刹车 3 秒解锁 |
| `ui_ButtonJoyMinus` | Button | 中心值 -10 并保存 | 返回 `CenterMinus` | 微调中心偏移 |
| `ui_ButtonJoyPlus` | Button | 中心值 +10 并保存 | 返回 `CenterPlus` | 微调中心偏移 |
| `ui_ButtonJoyReset` | Button | 恢复默认校准并保存 | 返回 `ResetCalibration` | 设回 center=2048、min=0、max=4095、deadzone=50 |

说明：当前固件仍保留一个临时设置浮层，按钮1短按可进入。你在 SquareLine 中放置正式校准页按钮后，固件会优先响应正式按钮。

### 3.4 系统页

建议 Screen 名：`ui_ScreenSystem`。用于确认固件版本、背光和升级流程。

| 对象名 | 类型 | 显示/范围 | 数据来源或动作 | 调试要点 |
| --- | --- | --- | --- | --- |
| `ui_LabelFirmware` | Label | `FW s3-remote-ui Jun 15 2026` | `firmwareVersion`、`buildDate` | 用于确认烧录的是新固件 |
| `ui_LabelBrightness` | Label | `BRI 140` | `displayBrightness` | 当前实际背光值 |
| `ui_SliderBrightness` | Slider | `20..255` | 用户拖动产生亮度请求 | 拖动后立即生效；固件会限幅 |
| `ui_LabelPowerMode` | Label | `ACTIVE` / `DIM` / `STANDBY` | `standbyMode`、`displayDimmed` | 判断是否进入降亮/待机 |
| `ui_LabelUpgrade` | Label | `USB: pio upload` | 固定提示 | 提醒使用 PlatformIO USB 烧录 |

### 3.5 临时设置浮层

该浮层不是 SquareLine 组件，由 `src/transmitter_s3/ui/ui.cpp::createSettingsPanel()` 运行时创建。按钮1短按进入/退出设置模式时显示。

| 对象 | 类型 | 显示/动作 |
| --- | --- | --- |
| `settingsPanel` | Container | 居中 210x170 半透明面板 |
| `settingsTitleLabel` | Label | `JOYSTICK SET` |
| `settingsCenterLabel` | Label | `CENTER 2048` |
| `settingsMinusButton` | Button | `-10`，中心值 -10 |
| `settingsCalButton` | Button | `CAL`，采样中心 |
| `settingsPlusButton` | Button | `+10`，中心值 +10 |
| `settingsCloseButton` | Button | `EXIT`，退出设置 |
| `settingsHintLabel` | Label | `Output locked` / `Re-arm after exit` |

## 4. S3 新 PCB 和当前实物差异

| 项目 | 当前 COM3 实物：`s3_transmitter` | 新 PCB：`s3_transmitter_new_pcb` |
| --- | --- | --- |
| LCD 电源 | `GPIO41` | `GPIO47` |
| LCD TE | `GPIO47` | `GPIO37` |
| AUX I2C | `GPIO18/GPIO19` | `GPIO39/GPIO38` |
| 档位输入 | `GPIO37/GPIO38/GPIO39` 三路数字输入 | `GPIO2` ADC 电阻分压 |
| 按钮1/按钮2 | `GPIO35/GPIO36` | `GPIO0/GPIO46` |
| 蜂鸣器 | `GPIO42` | `GPIO40` |
| 烧录环境 | `pio run -e s3_transmitter -t upload --upload-port COM3` | 新板打板后使用 `s3_transmitter_new_pcb` |

当前 COM3 旧板不要烧录 `s3_transmitter_new_pcb`。

## 5. C3 OLED 页面

C3 基础版没有 SquareLine 页面，只有 SSD1306 OLED 直接绘制。页面逻辑在 `src/transmitter/main.cpp::updateDisplay()`。

### 5.1 启动页

显示时机：`setupOLED()` 初始化 OLED 后。

| 行 | 显示内容 | 数据来源 | 含义 |
| --- | --- | --- | --- |
| 1 | `ESP-NOW Remote` | 固定文本 | 基础版遥控器启动 |
| 2 | `Initializing...` | 固定文本 | 正在初始化 |

### 5.2 主页面

显示时机：默认页面，`settingsMode == false`。

| 区域/行 | 显示格式 | 字段来源 | 含义/调试方式 |
| --- | --- | --- | --- |
| 第 1 行 | `[OK]  BAT:48.00V` | `connected`、`voltageValue` | 已收到接收端状态包；`BAT` 是接收端/VESC 电压 |
| 第 1 行替代 | `[LOST]` | `connected == false` | 500ms 未收到合法状态包 |
| 第 2 行 | `ARM SPD:2 THR: 120` | `transmitterArmed`、`speedLevel`、`joystickValue` | 已解锁、档位、油门方向和值 |
| 第 2 行替代 | `LOCK SPD:1 BRK: 900` | `transmitterArmed == false`、`joystickValue <= 0` | 未解锁、刹车方向和值 |
| 第 3 行可选 | `Hold BRK 3s` | `!transmitterArmed && joystickRawValue <= -900` | 摇杆已拉到最大刹车方向，保持 3 秒可解锁 |
| 大字号速度 | `23 KM` | `speedValue`、`connected` | 接收端回传速度，连接时显示 |
| 大字号速度替代 | `N/A` | `connected == false` | 断联时速度无效 |
| 电池行 | `SOC: 87% BAT:4.12V` | `cw2015Available`、`localBatteryPercent`、`localBatteryVoltage` | C3 本机 CW2015 电量和电压 |
| 电池行替代 | `N/A:  0% BAT:0.00V` | `cw2015Available == false` | C3 未检测到 CW2015 |
| 条形行 | `THR:====` / `BRK:====` | `joystickValue` | 油门/刹车大小条，最大约 10 个 `=` |

主页面字段对应变量：

| 变量 | 类型 | 来源 | UI 用途 |
| --- | --- | --- | --- |
| `connected` | `volatile bool` | 状态包超时判断 | `[OK]` / `[LOST]` |
| `voltageValue` | `volatile uint16_t` | 接收端状态包 `voltage` | 接收端/VESC 电压，除以 100 显示 |
| `transmitterArmed` | `bool` | 最大刹车保持 3 秒 | `ARM` / `LOCK` |
| `speedLevel` | `volatile uint8_t` | 三档开关 GPIO6/5/4 | `SPD:1..3` |
| `joystickRawValue` | `volatile int16_t` | 摇杆原始映射值 | 解锁提示判断 |
| `joystickValue` | `volatile int16_t` | 安全处理和斜率限制后的输出 | `THR/BRK` 和条形行 |
| `speedValue` | `volatile uint16_t` | 接收端状态包 `speed` | 大字号速度 |
| `cw2015Available` | `bool` | C3 本机 I2C 检测 | `SOC` / `N/A` 标签 |
| `localBatteryPercent` | `uint8_t` | C3 本机 CW2015 | 本机电量百分比 |
| `localBatteryVoltage` | `float` | C3 本机 CW2015 | 本机电池电压 |

### 5.3 设置页

显示时机：按钮1短按进入，`settingsMode == true`。

| 行 | 显示格式 | 字段来源 | 含义/调试方式 |
| --- | --- | --- | --- |
| 1 | `SETTINGS` | 固定文本 | 当前在设置页 |
| 2 | `Center:2048` | `joystickCenter` | 当前摇杆中心值 |
| 3 | `Raw:   0 Out:   0` | `joystickRawValue`、`joystickValue` | 原始映射输出和实际发送输出 |
| 4 | `B2: Cal center` | 固定文本 | 按钮2重新校准中心 |
| 5 | `B1: Exit` | 固定文本 | 按钮1退出设置页 |
| 6 | `Output locked` | 固定文本 | 设置页强制输出锁定 |

设置页行为：

| 操作 | 固件行为 |
| --- | --- |
| 按钮1短按 | 进入/退出设置页；退出后仍需重新刹车 3 秒解锁 |
| 按钮1长按 3 秒 | 发送接管请求，不进入设置页 |
| 按钮2短按 | 在设置页内重新采样摇杆中心 |
| 设置页内油门输出 | 固定为 0，不发送有效油门 |

## 6. C3 串口诊断字段

C3 串口诊断用于自动化测试脚本，不显示在 OLED 上。

| 命令 | 返回 | 用途 |
| --- | --- | --- |
| `DIAG PING` | `DIAG PONG role=transmitter protocol=2` | 确认串口和固件角色 |
| `DIAG STATUS` | `DIAG STATUS role=transmitter connected=1 rx=... lost=... faults=...` | 自动化连通性测试 |
| `DIAG SIMSTATUS -55 4800 23 2` | `DIAG OK simstatus` | 模拟接收端状态包 |

## 7. UI 调试流程

1. S3 新增组件时，先按本文档对象名在 SquareLine 创建对象。
2. 导出到 `src/transmitter_s3/ui/generated/`。
3. 编译当前实物：

```text
pio run -e s3_transmitter
```

4. 如果对象名错误，优先检查生成头文件中的 `extern lv_obj_t *...` 名称。
5. 对当前 COM3 旧板烧录：

```text
pio run -e s3_transmitter -t upload --upload-port COM3
```

6. 跑 S3 10 秒连通性测试：

```text
python tools/diagnostics/link_test.py --receiver-port COM4 --remote-port COM3
```

7. C3 OLED 字段调试时，先确认主页面/设置页是否符合上表，再用串口诊断命令确认协议状态。
