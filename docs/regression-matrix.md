# 回归测试矩阵

每次修改协议、安全逻辑、S3 UI 或硬件引脚后，按本矩阵复测。

## 自动化测试

| 项目 | 命令 | 通过标准 |
| --- | --- | --- |
| Native 单测 | `pio test -e native` | 全部 PASS |
| C3 发射端编译 | `pio run -e transmitter` | SUCCESS |
| 接收端编译 | `pio run -e receiver` | SUCCESS |
| S3 发射端编译 | `pio run -e s3_transmitter` | SUCCESS |

## 上传测试

| 设备 | 命令 | 通过标准 |
| --- | --- | --- |
| 接收端 | `pio run -e receiver -t upload --upload-port COM4` | 上传成功，重启后串口有启动日志 |
| S3 发射端 | `pio run -e s3_transmitter -t upload --upload-port COM3` | 上传成功，屏幕进入主界面 |
| C3 发射端 | `pio run -e transmitter -t upload --upload-port COM3` | 仅在需要验证基础版发射端时执行 |

## 连通性和稳定性测试

| 项目 | 命令 | 通过标准 |
| --- | --- | --- |
| 10 秒诊断连通性 | `python tools/diagnostics/link_test.py --receiver-port COM4 --remote-port COM3` | 两端 `DIAG PING` 成功，10 个左右样本均 `connected=1` |
| 30 分钟诊断稳定性 | `python tools/diagnostics/link_test.py --receiver-port COM4 --remote-port COM3 --long` | 必须手动触发；全程无掉线，最终 `PASS duration=1800s` |

## 实物功能

| 编号 | 功能 | 方法 | 通过标准 |
| --- | --- | --- | --- |
| HW-01 | 协议 v2 连接 | 接收端 COM4 和一个发射端同时上电 | 发射端显示 OK，接收端有控制日志 |
| HW-02 | 摇杆中位持久化 | S3 校准后断电重启 | 回中 throttle 为 0 附近 |
| HW-03 | 解锁保护 | 上电后直接推油门 | 未解锁前接收端输出保持中位 |
| HW-04 | 刹车解锁 | 拉满刹车约 3 秒 | 发射端进入 ARM，允许输出 |
| HW-05 | Failsafe | 发射端断电 | 接收端 throttle 清零，状态包置 `FS` |
| HW-06 | VESC 遥测 | 插拔 VESC UART 或关闭 VESC | 状态位 `VESC` 跟随变化 |
| HW-07 | 背光降亮度 | S3 静置 30 秒 | 背光降低，触摸/按钮/摇杆恢复 |
| HW-08 | MCU 温度 | 长时间运行 | 超过阈值时 UI 温度进入告警色 |
| HW-09 | 指南针 | 转动 S3 | 箭头随航向变化，无效时半透明 |
| HW-10 | 远程蜂鸣 | 按住按钮 2 超过 3 秒 | 接收端蜂鸣自动停止，松开后可再次触发 |
