# 2026-07-18 单芯片串口 HIL 本地验证

## 范围

本次完成接收器、C3 基础遥控器、S3 遥控器及新 PCB 环境的 HIL 构建隔离、固件协议、Python 桌面工具、
自动场景、报告和文档。用户确认硬件未接入，因此没有烧录或执行串口实机场景。

## 自动测试

- Python `unittest`：14/14 通过。
- PlatformIO Native：70/70 通过。
- `python -m compileall -q hil tools/diagnostics/link_test.py`：通过。
- 6 个 JSON 场景语法与版本检查：通过。
- tkinter 仪表板无串口启动存活 4 秒：通过；未进行人工像素级目视检查。
- `git diff --check`：通过。

## Clean build

| 环境 | RAM | Flash | 结果 |
|---|---:|---:|---|
| `transmitter` | 38,396 / 327,680 | 770,900 / 1,310,720 | SUCCESS |
| `receiver` | 38,188 / 327,680 | 757,282 / 1,310,720 | SUCCESS |
| `s3_transmitter` | 132,560 / 327,680 | 1,422,373 / 3,342,336 | SUCCESS |
| `s3_transmitter_new_pcb` | 132,560 / 327,680 | 1,422,337 / 3,342,336 | SUCCESS |
| `transmitter_hil` | 38,620 / 327,680 | 781,718 / 1,310,720 | SUCCESS |
| `receiver_hil` | 38,468 / 327,680 | 789,668 / 1,310,720 | SUCCESS |
| `s3_transmitter_hil` | 132,824 / 327,680 | 1,437,465 / 3,342,336 | SUCCESS |
| `s3_transmitter_new_pcb_hil` | 132,824 / 327,680 | 1,437,425 / 3,342,336 | SUCCESS |

## 二进制隔离

- 4 个正式 `firmware.bin` 均不存在 `line_too_long`、`outputs_unlocked` 和 HIL 固件标识。
- 4 个 HIL `firmware.bin` 均包含协议错误码、输出锁字段和 HIL 状态标识。

## 未执行

- COM3/COM4/COM5 烧录。
- HIL `PING/STATUS` 实机握手。
- 输出锁 MCU 引脚实测。
- ESP-NOW、VESC UART、实体输入、传感器和显示实测。
- 10 秒及 30 分钟连通稳定性测试。
