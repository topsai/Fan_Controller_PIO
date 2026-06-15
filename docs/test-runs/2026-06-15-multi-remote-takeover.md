# 2026-06-15 多遥控器主动接管

## 目标

同步更新基础版 C3 遥控器 `COM5`、S3 遥控器 `COM3` 和接收端 `COM4`，验证多遥控器不会自动抢占，必须按钮1长按 3 秒发送接管请求。

## 修改摘要

- 协议新增 `CONTROL_FLAG_TAKEOVER_REQUEST = 0x01`。
- 接收端新增 active 控制源锁定、ignored 计数和 `DIAG SIMCTRLFROM`。
- C3/S3 按钮1改为短按设置页、长按 3 秒接管。
- 诊断脚本支持 `--remote-a COM5 --remote-b COM3` 多遥控器切换测试。

## 验证

| 项目 | 结果 |
| --- | --- |
| `python -m py_compile tools/diagnostics/link_test.py` | PASS |
| `python tools/diagnostics/link_test.py --help` | PASS |
| `pio test -e native` | PASS，46/46 |
| `pio run -e receiver` | SUCCESS |
| `pio run -e transmitter` | SUCCESS |
| `pio run -e s3_transmitter` | SUCCESS |
| `pio run -e receiver -t upload --upload-port COM4` | SUCCESS，MAC `AC:EB:E6:44:C5:90` |
| `pio run -e transmitter -t upload --upload-port COM5` | SUCCESS，MAC `AC:EB:E6:44:D5:54` |
| `pio run -e s3_transmitter -t upload --upload-port COM3` | SUCCESS，MAC `48:CA:43:9A:A9:B0` |
| `python tools/diagnostics/link_test.py --receiver-port COM4 --remote-port COM5` | PASS，10 秒，10 samples，active=`c3` |
| `python tools/diagnostics/link_test.py --receiver-port COM4 --remote-port COM3` | PASS，10 秒，10 samples，active=`s3` |
| `python tools/diagnostics/link_test.py --receiver-port COM4 --remote-a COM5 --remote-b COM3` | PASS，10 秒，6 switches |

## 观察

- C3 与 S3 同时在线时，接收端 `ignored` 计数会增长，这是非 active 遥控器普通包被拒绝的预期行为。
- S3 单遥控器诊断前后 `faults` 为累计计数，本轮连通性判断以 `connected=1` 和 active 切换为准。
- 30 分钟稳定性测试未运行；按项目规则只在显式指定 `--long` 时执行。
