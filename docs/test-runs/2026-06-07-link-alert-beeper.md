# 2026-06-07 Link Alert Beeper Test Run

## 1. 功能目标

遥控器失效、未打开或断电后，接收器每隔 2 秒发出一次提示音。

## 2. 实现内容

接收端新增链路异常提示逻辑：

| 参数 | 值 | 说明 |
|---|---:|---|
| `LINK_ALERT_INTERVAL` | 2000ms | 周期提示间隔 |
| `LINK_ALERT_BEEP_MS` | 200ms | 每次提示音时长 |

触发条件：

- `connected == false`
- 或 `failsafeActive == true`

静默条件：

- `connected == true`
- 且 `failsafeActive == false`

## 3. 自动化验证

| 项目 | 结果 |
|---|---|
| `pio test -e native` | PASS，8/8 |
| `pio run -e transmitter` | SUCCESS |
| `pio run -e receiver` | SUCCESS |

新增测试：

```text
test_receiver_link_alert_beeps_immediately_then_every_two_seconds
test_receiver_link_alert_is_silent_when_connected_and_not_failsafe
```

## 4. 固件上传

| 目标 | 命令 | 结果 |
|---|---|---|
| 发射端 COM3 | `pio run -e transmitter -t upload --upload-port COM3` | SUCCESS |
| 接收端 COM10 | `pio run -e receiver -t upload --upload-port COM10` | SUCCESS |

## 5. 硬件日志验证

### 在线状态

接收端抓取 10 秒。日志显示接收端持续收到控制数据：

```text
THR:   0 SPD:3 BTN:00
PWM value: 76 (throttle=0)
```

未出现：

```text
遥控器未连接，接收端提示音
```

结论：连接正常时不会触发周期提示音。

### 离线状态

接收端抓取 12 秒，未触发周期提示音。日志仍显示持续收到发射端数据。

结论：本轮未形成真实断联条件，离线断电提示音待复测。

## 6. 待复测

确保发射端真正断电或断开发射端供电后，接收端应每 2 秒出现一次：

```text
遥控器未连接，接收端提示音
```

并应能听到接收端蜂鸣器每 2 秒响约 200ms。
