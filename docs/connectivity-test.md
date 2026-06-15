# 连通性和稳定性测试

本文档记录新增的串口诊断注入测试。它用于补齐“编译/烧录通过不等于实机连通”的缺口。

## 测试能力

固件新增串口诊断命令：

- `DIAG PING`：确认电脑能和设备串口通信。
- `DIAG STATUS`：读取设备当前诊断状态。
- 接收端 `DIAG SIMCTRL <throttle> <speedLevel> <buttons> <flags>`：向接收端注入模拟控制数据。
- 遥控器 `DIAG SIMSTATUS <rssi> <voltageX100> <speed> <status>`：向遥控器注入模拟接收端状态数据。

PC 端脚本：

```powershell
python tools/diagnostics/link_test.py --receiver-port COM4 --remote-port COM3
```

默认运行 10 秒。脚本会每秒向接收端和遥控器发送模拟数据，并检查双方 `DIAG STATUS` 都保持 `connected=1`。

## 30 分钟稳定性测试

30 分钟测试不会默认触发，必须手动指定：

```powershell
python tools/diagnostics/link_test.py --receiver-port COM4 --remote-port COM3 --long
```

等价的自定义长时测试也必须显式允许：

```powershell
python tools/diagnostics/link_test.py --receiver-port COM4 --remote-port COM3 --duration 1800 --allow-long
```

## 测试边界

这个测试证明：

- 电脑能同时和接收端、遥控器串口通信。
- 两端固件都能接收诊断命令。
- 接收端能处理模拟控制数据并保持连接状态。
- 遥控器能处理模拟状态数据并保持连接状态。
- 10 秒或 30 分钟期间诊断状态没有掉线。

这个测试不等同于真实 RF 链路测试。真实 ESP-NOW 连通性仍需要观察：

- 接收端真实 `THR/SPD/BTN` 日志随遥控器输入变化。
- 遥控器 UI/OLED 从 `LOST` 变为 `OK`。
- 接收端 PWM 随摇杆变化。
- 遥控器能收到接收端真实电压/速度/RSSI 回包。
