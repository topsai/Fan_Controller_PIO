# 2026-06-15 状态包序号超时重置修复

## 现象

用户反馈接收器一直在断线报警。串口排查时接收端 `DIAG STATUS` 显示 `connected=1`，说明接收端仍在收到合法控制包；同时 C3/S3 遥控器侧显示 `[LOST]` 或 `connected=0`。

## 根因

接收端重新烧录或重启后，状态包 `sequence` 从 0 重新开始；遥控器端仍保留旧的 `lastStatusSequence`。由于状态包序号检查要求“新序号必须比上一包更新鲜”，遥控器会持续拒收接收端从 0 开始的新状态包，并保持 `[LOST]` 报警。

## 修复

- 新增 `resetSequenceAfterConnectionTimeout()`。
- C3 基础版遥控器连接超时后清除状态包序号记忆。
- S3 高级遥控器连接超时后清除状态包序号记忆，并清零状态包丢包估计。

## 验证

| 项目 | 结果 |
| --- | --- |
| `pio test -e native` | PASS，47/47 |
| `pio run -e transmitter` | SUCCESS |
| `pio run -e s3_transmitter` | SUCCESS |
| `pio run -e transmitter -t upload --upload-port COM5` | SUCCESS |
| `pio run -e s3_transmitter -t upload --upload-port COM3` | SUCCESS |
| `python tools/diagnostics/link_test.py --receiver-port COM4 --remote-port COM5` | PASS，10 秒 |
| `python tools/diagnostics/link_test.py --receiver-port COM4 --remote-port COM3` | PASS，10 秒 |
| `python tools/diagnostics/link_test.py --receiver-port COM4 --remote-a COM5 --remote-b COM3` | PASS，10 秒，6 switches |

## 备注

多遥控器同时开机时，只有 active 遥控器会收到接收端状态回包。非 active 遥控器如果仍按“断线”蜂鸣，这是下一步产品行为优化点，不代表接收端断线。
