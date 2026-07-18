# Fan Controller PIO

ESP32-C3/ESP32-S3 滑板遥控器与 VESC 接收器固件。项目包含 C3 基础遥控器、C3 接收器、S3 高级遥控器
以及对应的单芯片串口 HIL 测试环境。

## 快速入口

- 当前状态与换机接续：[docs/current-status.md](docs/current-status.md)
- 功能与参数：[docs/README.md](docs/README.md)
- ESP-NOW v2 协议：[docs/protocol-v2.md](docs/protocol-v2.md)
- HIL 桌面测试：[docs/hil-testing.md](docs/hil-testing.md)
- 测试与发布流程：[docs/workflow.md](docs/workflow.md)

## 常用命令

```powershell
pio test -e native
pio run -e transmitter
pio run -e receiver
pio run -e s3_transmitter

python -m pip install -r hil/requirements.txt
python -m hil.dashboard
```

HIL 固件上电默认锁定真实输出。连接 VESC、电机或其他负载前，请先阅读 HIL 文档中的安全要求。
