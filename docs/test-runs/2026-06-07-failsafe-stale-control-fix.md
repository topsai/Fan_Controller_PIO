# 2026-06-07 Failsafe Stale Control Fix Test Run

## 1. 故障描述

遥控器突然失效或断电后，接收端仍可能继续使用断联前的数据。

## 2. 根因判断

接收端 `checkFailsafe()` 原逻辑只清零 `throttle`，没有统一清理：

- `speedLevel`
- `buttons`
- `connected`

因此断联前的按钮状态、档位状态可能继续保留。

## 3. 修复内容

新增 `applyReceiverFailsafe()`，进入 failsafe 后统一切换到安全状态：

| 字段 | 安全值 |
|---|---|
| `throttle` | `0` |
| `speedLevel` | `1` |
| `buttons` | `0` |
| `connected` | `false` |
| `failsafeActive` | `true` |

## 4. 自动化验证

| 项目 | 结果 |
|---|---|
| `pio test -e native` | PASS，6/6 |
| `pio run -e transmitter` | SUCCESS |
| `pio run -e receiver` | SUCCESS |

新增测试：

```text
test_receiver_failsafe_clears_all_stale_control_inputs
```

该测试先模拟断联前状态 `throttle=800, speedLevel=3, buttons=0x03, connected=true`，再确认 failsafe 后所有控制输入回到安全状态。

## 5. 固件上传

| 目标 | 命令 | 结果 |
|---|---|---|
| 发射端 COM3 | `pio run -e transmitter -t upload --upload-port COM3` | SUCCESS |
| 接收端 COM10 | `pio run -e receiver -t upload --upload-port COM10` | SUCCESS |

## 6. 硬件复测

接收端串口抓取 18 秒，未触发 failsafe。日志持续显示：

```text
THR:   0 SPD:1 BTN:00
PWM value: 76 (throttle=0)
```

判断：本轮测试期间接收端仍持续收到发射端数据，未能形成真实断联条件。

## 7. 待复测

下一轮应确保发射端真正断电或断开发射端供电超过 3 秒，再确认接收端出现：

```text
失控保护启动！
THR:   0 SPD:1 BTN:00 [FAILSAFE]
PWM value: 76 (throttle=0)
```
