# Progress Log

## 2026-06-07

### 目标

建立 Fan Controller PIO 项目的文档体系，记录当前 Arduino 可运行代码迁移后的功能基线。

### 已完成

- 梳理发射端和接收端功能。
- 确认当前代码中的 MAC、引脚、VESC 速度系数为真实可用基线。
- 建立文档入口和以下主题文档：
  - 需求规格
  - 硬件清单和引脚
  - ESP-NOW 协议
  - 状态机
  - 参数表
  - 测试计划
  - 单元测试规划
  - bug/维护项
  - 设计决策

### 当前状态

文档体系已建立。代码尚未修改。

### 下一步

1. 根据文档确认是否需要调整目录或内容。
2. 进入代码整理前，优先实现发射端摇杆开机中位校准。
3. 在修改协议前，决定是否删除 `StatusPacket.motorPWM[4]`。

## 2026-06-07 硬件上传和基础联调

### 目标

将当前 PIO 固件上传到实际发射端和接收端硬件，并按测试计划做基础功能验证。

### 已完成

- 确认串口：
  - COM3：发射端，MAC `AC:EB:E6:44:D5:54`
  - COM10：接收端，MAC `AC:EB:E6:44:C5:90`
- `pio run -e transmitter` 编译通过。
- `pio run -e receiver` 编译通过。
- 发射端已上传到 COM3。
- 接收端已上传到 COM10。
- 为 ESP32-C3 USB Serial/JTAG 串口日志启用 `ARDUINO_USB_MODE=1` 和 `ARDUINO_USB_CDC_ON_BOOT=1`。
- 验证两端启动日志、MAC 绑定、ESP-NOW 连接。
- 验证摇杆正向和反向输入能传到接收端并影响 PWM 输出。
- 验证按钮 2 能通过协议传到接收端。

### 未完成

- 未捕获到档位 1/2 的切换日志。
- 未捕获到按钮 1 的 `BTN:01` 日志。
- 失控保护未触发，接收端日志显示仍持续收到发射端数据。

### 记录

详细结果见 `test-runs/2026-06-07-hardware-bringup.md`。

## 2026-06-07 不完善代码修复

### 目标

修复硬件联调后发现的高优先级代码问题，优先覆盖 PWM 映射、摇杆中位、遥测初始化、接收端调试输出和失控蜂鸣。

### 已完成

- 新增 `include/control_logic.h`，把 PWM 映射、档位限幅、摇杆映射、摇杆中心平均值计算抽为可测试纯逻辑。
- 新增 `test/test_control_logic/test_main.cpp`，覆盖：
  - VESC Servo/PPM duty 范围
  - 1/2/3 档限幅
  - 非法档位默认 1 档
  - 摇杆校准中心和死区
  - 摇杆中心采样平均值
- 发射端改为上电采样 64 次校准摇杆中位。
- 接收端 PWM 输出改用公共映射函数。
- 接收端串口日志改为打印实际限幅后的 throttle。
- 接收端 `StatusPacket` 初始化清零，避免冗余字段参与校验和时携带未初始化字节。
- 接收端失控蜂鸣不再被按钮 2 的 `noTone()` 逻辑立即打断。

### 验证

- `pio test -e native`：5/5 PASS。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e receiver`：SUCCESS。

### 下一步

1. 上传新固件到 COM3/COM10。
2. 复测摇杆回中值是否接近 0。
3. 复测接收端串口是否显示实际 throttle。
4. 复测断开发射端后的失控保护和蜂鸣。

## 2026-06-07 GitHub 同步流程要求

### 目标

记录后续每次代码更新的固定流程：测试、文档、上传固件、同步推送到 GitHub。

### 已完成

- 新增 `docs/workflow.md`。
- 在 `docs/README.md` 中加入 workflow 索引和维护规则。
- 补充 `.gitignore`，忽略 `.pio/`、`.cache/` 和 `compile_commands.json`。

### 当前阻塞

- 当前目录不是 git 仓库。
- 本机未安装 GitHub CLI `gh`。
- 尚未确认 GitHub 远程仓库名或 URL。

### 下一步

1. 安装并登录 GitHub CLI。
2. 确认 GitHub 仓库名或远程 URL。
3. 初始化本地 git 仓库，做首次提交并推送。

## 2026-06-07 GitHub 首次推送确认

### 目标

确认项目已经通过 VSCode 推送到 GitHub，并更新流程文档中的 GitHub 状态。

### 已完成

- 确认当前目录已经是 git 仓库。
- 确认远程仓库为 `https://github.com/topsai/Fan_Controller_PIO.git`。
- 确认本地 `main` 跟踪 `origin/main`。
- 确认当前提交为 `f9ef724 first commit`。
- 更新 `docs/workflow.md`，移除“尚未初始化 git 仓库”的阻塞说明。

### 当前状态

本地 `main` 已连接 `origin/main`。后续更新应按 `docs/workflow.md` 执行测试、文档、上传固件、提交并推送。

## 2026-06-07 接收端断联安全状态修复

### 目标

修复遥控器突然失效或断电后，接收端继续沿用断联前控制数据的风险。

### 根因

接收端原 `checkFailsafe()` 只设置 `throttle=0` 和 `failsafeActive=true`，没有统一清理 `speedLevel`、`buttons`、`connected` 等状态。因此断联前的按钮或档位状态可能继续影响接收端逻辑。

### 已完成

- 新增 `ReceiverControlState` 和 `applyReceiverFailsafe()`。
- failsafe 状态统一设置为：
  - `throttle=0`
  - `speedLevel=1`
  - `buttons=0`
  - `connected=false`
  - `failsafeActive=true`
- 接收端 `checkFailsafe()` 已接入该安全状态函数。
- 新增单元测试 `test_receiver_failsafe_clears_all_stale_control_inputs`。
- 新固件已上传：
  - COM3：发射端
  - COM10：接收端

### 验证

- `pio test -e native`：6/6 PASS。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e receiver`：SUCCESS。
- `pio run -e transmitter -t upload --upload-port COM3`：SUCCESS。
- `pio run -e receiver -t upload --upload-port COM10`：SUCCESS。

### 硬件复测状态

已抓取接收端日志，但本轮未触发 failsafe；日志显示接收端仍持续收到发射端数据。需要在下一轮测试中确保发射端真正断电或断开发射端供电超过 3 秒。

## 2026-06-07 接收端链路异常周期提示音

### 目标

遥控器未打开、断电或失效时，接收端每隔 2 秒发出一次提示音。

### 已完成

- 新增 `shouldEmitReceiverLinkAlert()`，控制链路异常提示音节流。
- 接收端新增参数：
  - `LINK_ALERT_INTERVAL=2000`
  - `LINK_ALERT_BEEP_MS=200`
- 接收端在 `!connected` 或 `failsafeActive` 时，每 2 秒蜂鸣 200ms。
- 连接正常且非 failsafe 时不发出该周期提示音。
- 新增单元测试：
  - `test_receiver_link_alert_beeps_immediately_then_every_two_seconds`
  - `test_receiver_link_alert_is_silent_when_connected_and_not_failsafe`
- 新固件已上传：
  - COM3：发射端
  - COM10：接收端

### 验证

- `pio test -e native`：8/8 PASS。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e receiver`：SUCCESS。
- `pio run -e transmitter -t upload --upload-port COM3`：SUCCESS。
- `pio run -e receiver -t upload --upload-port COM10`：SUCCESS。
- 在线状态抓取接收端 10 秒，未出现“遥控器未连接，接收端提示音”，符合连接正常时静默要求。

### 硬件复测状态

离线断电抓取 12 秒未触发提示音，日志显示接收端仍持续收到发射端数据。下一轮需要确保发射端真正断电或断开发射端供电后再复测。
