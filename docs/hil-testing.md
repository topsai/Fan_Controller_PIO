# 单芯片串口 HIL 实时测试

## 架构与边界

HIL 固件运行目标角色原有的 `setup()`、`loop()`、ESP-NOW 解码、输入映射、解锁、状态机、NVS、显示、
传感器、failsafe 和输出代码。串口只在真实输入边界注入数据，并在物理输出末端增加安全锁。

每次只需要一块目标板，不需要第二块模拟 MCU。自动测试可以验证协议、业务状态、期望输出和 MCU 引脚写入值，
不能证明天线、真实 ESP-NOW 射频、实体按键、屏幕像素、传感器接线、VESC UART 电气或电机动作正确。

## 正式与 HIL 环境

| 角色 | 正式环境 | HIL 环境 |
|---|---|---|
| C3 接收器 | `receiver` | `receiver_hil` |
| C3 基础遥控器 | `transmitter` | `transmitter_hil` |
| S3 当前硬件 | `s3_transmitter` | `s3_transmitter_hil` |
| S3 新 PCB | `s3_transmitter_new_pcb` | `s3_transmitter_new_pcb_hil` |

正式环境不包含 `HIL` 命令、输入注入、状态响应和输出解锁接口。HIL 代码由 `FAN_CONTROLLER_HIL` 隔离。

## 安装与启动

```powershell
cd C:\Users\John\Desktop\Code\Fan_Controller_PIO
python -m pip install -r hil/requirements.txt
python -m hil.dashboard
```

也可使用 `python -m hil.dashboard --port COM4`。端口不存在或连接失败时窗口仍保持完整，实时字段显示“不可用”，
曲线不产生假数据。断开后窗口不关闭，旧实时值会被清空。

烧录示例：

```powershell
pio run -e receiver_hil -t upload --upload-port COM4
pio run -e transmitter_hil -t upload --upload-port COM5
pio run -e s3_transmitter_hil -t upload --upload-port COM3
```

## 串口协议

请求为 `HIL <sequence> <command> [arguments...]`，响应为一行 JSON。每个请求恰好对应一个同序号响应。
最大请求行 191 字节，解析器不使用 Arduino `String` 或动态 JSON。

稳定错误码包括：`empty`、`line_too_long`、`invalid_prefix`、`invalid_sequence`、`unknown_command`、
`invalid_argument`、`unsupported` 和 `frame_rejected`。

### 通用命令

```text
PING
STATUS
OUTPUTS LOCK
OUTPUTS UNLOCK
RESET
REBOOT
NVS CLEAR
```

`NVS CLEAR` 只支持拥有持久配置的遥控器；接收器返回 `unsupported`。

### 接收器命令

```text
REMOTE FRAME <c3|s3> <hex>
REMOTE CONTROL <c3|s3> <throttle> <speed-level> <buttons> <flags>
REMOTE REPEAT <count>
REMOTE INVALID <c3|s3> <crc|unknown_address|unknown_command|truncated|stale>
VESC PHYSICAL
VESC VALUE <voltage-x100> <rpm>
VESC FAULT
```

`REMOTE FRAME` 与真实 ESP-NOW 回调共用帧解码入口，覆盖包长、帧头、版本、CRC、序号、固定 MAC、活动控制源、
三包稳定门限和 failsafe。`REMOTE CONTROL` 先构造合法 v2 原始帧，再进入同一入口。

### 遥控器命令

```text
INPUT JOYSTICK <0..4095|PHYSICAL>
INPUT SPEED <1..3|PHYSICAL>
BUTTON <1|2> CLICK
BUTTON <1|2> HOLD <milliseconds>
STATUS FRAME <hex>
```

按键注入发生在按键电平读取边界，继续执行正式固件的消抖、短按、长按、设置页、接管和校准逻辑。
状态帧继续执行版本、CRC、序号和丢包统计。

### S3 传感器命令

```text
SENSOR CW2015 VALUE <voltage>
SENSOR BMP280 VALUE <temperature-c>
SENSOR COMPASS VALUE <heading-deg>
SENSOR MCU VALUE <temperature-c>
SENSOR <name> FAULT
SENSOR <name> PHYSICAL
```

## 状态字段

通用字段包括固件、HIL 协议版本、角色、运行时间、最近命令、输出锁和连接状态。

- 接收器：活动控制源、稳定包数、原始帧结果、油门、档位、按钮、VESC、failsafe、协议故障、诊断计数、
  `expected_outputs` 和 `actual_outputs`。
- 遥控器：输入模式、摇杆 ADC/映射/输出、校准值、档位、按钮、解锁、设置页、接收器状态、电池和输出状态。
- S3：额外包含 CW2015、BMP280、指南针、MCU 温度和显示亮度。

## 输出安全锁

HIL 上电、复位和重启默认锁定。10 秒没有合法 HIL 命令时自动重新锁定。

- 接收器锁定时，GPIO4 PWM 固定为中位 duty `76`，蜂鸣器关闭；内部仍计算期望 PWM。
- 遥控器锁定时，不发送真实 ESP-NOW 控制包，蜂鸣器关闭；显示和内部状态继续更新。
- Python 退出、断开和场景清理时均发送 `OUTPUTS LOCK`。
- 自动场景默认不允许解锁。场景必须声明 `requires_outputs: true`，并在界面中人工确认。

初次测试必须断开电机和危险负载。真实负载测试需要人员在场并确认 VESC 的 Servo/PWM 中位配置与 duty `76` 一致。

## 自动场景与报告

场景位于 `hil/scenarios/`：协议错误、安全锁、failsafe、多遥控器接管、错误帧、遥控输入和 S3 传感器均有独立场景。

```powershell
python -m hil.run_scenario --port COM4 hil/scenarios/receiver_safety.json
```

报告写入 Git 忽略的 `hil/reports/`，格式为 JSON、CSV、Markdown，不生成 HTML。每一步记录请求、响应、断言、
耗时、结果、失败原因和最终锁定清理结果。

## 实机检查清单

- 正式固件下真实 C3/S3 ESP-NOW 连接与 10 秒、30 分钟稳定性。
- 天线距离、遮挡、干扰和两遥控器主动接管。
- VESC UART 电压/RPM 与 GPIO4 示波器波形。
- 实体摇杆、档位、按键消抖和长按时序。
- OLED/LVGL 像素、触摸、亮度和中文字体。
- CW2015、BMP280、QMC5883L、MCU 温度的真实读数和故障恢复。
- NVS 校准/亮度掉电恢复。
- 真实蜂鸣器、LED、VESC 和电机动作。

协议注入通过不能替代上述硬件检查。
