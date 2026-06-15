# 回归测试矩阵

每次修改协议、安全逻辑、S3 UI 或硬件引脚后，按本矩阵复测。

## 自动化测试

| 项目 | 命令 | 通过标准 |
| --- | --- | --- |
| Native 单测 | `pio test -e native` | 全部 PASS |
| 接收端编译 | `pio run -e receiver` | SUCCESS |
| C3 基础版发射端编译 | `pio run -e transmitter` | SUCCESS |
| S3 高级发射端编译 | `pio run -e s3_transmitter` | SUCCESS |
| 诊断脚本语法 | `python -m py_compile tools/diagnostics/link_test.py` | 无错误 |

## 上传测试

| 设备 | 命令 | 通过标准 |
| --- | --- | --- |
| 接收端 | `pio run -e receiver -t upload --upload-port COM4` | 上传成功，重启后串口有启动日志 |
| C3 基础版发射端 | `pio run -e transmitter -t upload --upload-port COM5` | 上传成功，OLED 进入主界面 |
| S3 高级发射端 | `pio run -e s3_transmitter -t upload --upload-port COM3` | 上传成功，屏幕进入主界面 |

## 连通性和稳定性

| 项目 | 命令 | 通过标准 |
| --- | --- | --- |
| C3 10 秒诊断连通性 | `python tools/diagnostics/link_test.py --receiver-port COM4 --remote-port COM5` | 双方 `DIAG PING` 成功，样本均 `connected=1` |
| S3 10 秒诊断连通性 | `python tools/diagnostics/link_test.py --receiver-port COM4 --remote-port COM3` | 双方 `DIAG PING` 成功，样本均 `connected=1` |
| 多遥控器主动切换 | `python tools/diagnostics/link_test.py --receiver-port COM4 --remote-a COM5 --remote-b COM3` | 普通包不能抢占；带接管 flag 后 active 在 C3/S3 间切换 |
| 30 分钟诊断稳定性 | `python tools/diagnostics/link_test.py --receiver-port COM4 --remote-port COM5 --long` | 必须手动触发；全程无掉线，最终 `PASS duration=1800s` |
| 30 分钟轮换稳定性 | `python tools/diagnostics/link_test.py --receiver-port COM4 --remote-a COM5 --remote-b COM3 --long` | 必须手动触发；轮换过程中无掉线 |

## 实物功能

| 编号 | 功能 | 方法 | 通过标准 |
| --- | --- | --- | --- |
| HW-01 | 协议 v2 连接 | 接收端 COM4 和一个发射端同时上电 | 发射端显示 OK，接收端有控制日志 |
| HW-02 | 摇杆中位持久化 | S3 校准后断电重启 | 回中 throttle 在 0 附近 |
| HW-03 | 解锁保护 | 上电后直接推油门 | 未解锁前接收端输出保持中位 |
| HW-04 | 刹车解锁 | 拉满刹车约 3 秒 | 发射端进入 ARM，允许输出 |
| HW-05 | Failsafe | 当前 active 发射端断电 | 接收端 throttle 清零，状态包置 `FS`，active 控制源释放 |
| HW-06 | 多遥控器锁定 | 两台遥控器同时开机 | 非 active 遥控器普通包不会抢占 |
| HW-07 | 主动接管 | 非 active 遥控器按钮1长按 3 秒 | 接收端输出先归零，3 包稳定后切换状态回包目标 |
| HW-08 | 按钮1短按 | 按钮1短按并释放 | 进入/退出设置页，不发送接管请求 |
| HW-09 | VESC 遥测 | 插拔 VESC UART 或关闭 VESC | 状态位 `VESC` 跟随变化 |
| HW-10 | 背光降亮度 | S3 静置 30 秒 | 背光降低，触摸/按钮/摇杆恢复 |
| HW-11 | MCU 温度 | 长时间运行 | 显示类似 `43.2C`，超过阈值时进入警告色 |
| HW-12 | 指南针 | 转动 S3 | 箭头随航向变化，无效时半透明 |
| HW-13 | 远程蜂鸣 | 按住按钮2超过 3 秒 | 接收端蜂鸣自动停止，松开后可再次触发 |
