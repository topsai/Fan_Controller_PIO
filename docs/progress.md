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

首次离线断电抓取 12 秒未触发提示音，日志显示接收端仍持续收到发射端数据。随后手动关闭发射器复测，接收端发出短促间隔滴滴声，链路异常周期提示音已通过人工听觉验证。

## 2026-06-07 使用说明整理

### 目标

把已有使用方法和功能整理成一份面向日常使用的说明，方便查看怎么开机、怎么操作、有哪些提示音和保护行为。

### 已完成

- 新增 `docs/user-guide.md`。
- 在 `docs/README.md` 增加使用指南入口。
- 更新链路异常提示音测试记录，补充手动关闭发射器后的实际听觉验证结果。
- 更新 `docs/bugs.md` 中 BUG-010 的验证状态。

### 覆盖内容

- 系统组成和端口约定。
- 正常开机和使用步骤。
- OLED 显示含义。
- 摇杆、三档开关、按钮功能。
- 接收端 PWM/Servo 输出和 VESC 遥测。
- 发射端/接收端提示音和报警。
- 失控保护安全状态。
- LED 状态。
- 常用编译、上传、测试、串口命令。
- 常见问题排查。

## 2026-06-08 接收端偶发长鸣修复

### 目标

排查并修复接收端运行一段时间后蜂鸣器偶发长鸣的问题。

### 根因判断

接收端蜂鸣器有多条触发路径，其中启动提示、失控提示和链路异常提示都带固定时长；只有按钮 2 远程蜂鸣使用无时长 `tone(BUZZER_PIN, 2000)`。如果接收端持续识别到异常或卡住的 `BTN:02` 状态，就会一直长鸣。

### 已完成

- 新增 `shouldAllowRemoteHorn()`，把远程蜂鸣抽成可测试逻辑。
- 接收端新增 `REMOTE_HORN_MAX_MS=3000`，按钮 2 单次远程蜂鸣最长 3 秒。
- 按钮 2 松开后复位超时保护，可以再次按下重新触发。
- 超时停止时，接收端串口输出 `远程蜂鸣超时保护，已停止`。
- 新增单元测试：
  - `test_remote_horn_has_maximum_continuous_duration`
  - `test_remote_horn_resets_after_button_release`
- 更新使用说明、测试计划和 bug 记录。

### 验证

- `pio test -e native`：10/10 PASS。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e receiver`：SUCCESS。
- `pio run -e transmitter -t upload --upload-port COM3`：SUCCESS。
- `pio run -e receiver -t upload --upload-port COM10`：SUCCESS。

### 硬件复测状态

新固件已上传到 COM3 和 COM10。接收端串口启动日志可见，并抓取到 `BTN:02` 后输出 `远程蜂鸣超时保护，已停止`，说明接收端已执行 3 秒远程蜂鸣超时保护。由于原始现象为约 1 小时左右偶发，仍建议后续长时间通电观察。

## 2026-06-08 提示音频率区分

### 目标

不同提示音使用不同频率，避免所有场景都使用 2000Hz 导致听觉上难以区分。

### 已完成

- 新增 `include/beep_profiles.h`，集中定义提示音频率。
- 发射端：
  - 按键提示：1800Hz
  - 开机提示：2200Hz
  - 接收端断线报警：1200Hz
  - 低电量报警：700Hz
- 接收端：
  - 开机提示：2200Hz
  - 遥控器未连接/断电/失效提示：1200Hz
  - 失控保护提示：900Hz
  - 按钮 2 远程蜂鸣：2600Hz
- 新增单元测试 `test_beep_profiles_use_distinct_frequencies`。
- 更新 `docs/parameters.md` 和 `docs/user-guide.md`。

### 验证

- `pio test -e native`：11/11 PASS。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e receiver`：SUCCESS。
- `pio run -e transmitter -t upload --upload-port COM3`：SUCCESS。
- `pio run -e receiver -t upload --upload-port COM10`：SUCCESS。

## 2026-06-08 接收端连接成功提示音

### 目标

遥控器蜂鸣器暂未接线，因此连接成功提示音先放在接收端。

### 已完成

- 新增连接成功频率 `BEEP_FREQ_CONNECTED=2400Hz`。
- 新增 `shouldSignalReceiverConnectionSuccess()`，只在接收端从未连接或 failsafe 状态恢复为连接时触发。
- 接收端收到合法控制包后，如果是连接恢复边沿，主循环播放 2400Hz、160ms 短提示音。
- 蜂鸣播放不在 ESP-NOW 回调内直接执行，只在回调内置 `connectionBeepPending` 标志。
- 新增 `buzzerHoldUntil`，避免带时长提示音被主循环后续 `noTone()` 立即打断。
- 新增单元测试 `test_receiver_connection_success_only_on_reconnect_edge`。

### 验证

- `pio test -e native`：12/12 PASS。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e receiver`：SUCCESS。
- `pio run -e transmitter -t upload --upload-port COM3`：SUCCESS。
- `pio run -e receiver -t upload --upload-port COM10`：SUCCESS。
- COM10 串口已抓到 `遥控器连接成功，接收端提示音`。

## 2026-06-08 ESP32-S3R8 屏幕/触摸最小探针

### 目标

把另一块 ESP32-S3R8 触摸屏开发板纳入当前项目，先用 Arduino + PlatformIO 最小化验证屏幕点亮和触摸读取，作为后续 LVGL 高级版遥控器的基础。

### 已完成

- 新增 PlatformIO 环境 `s3_lvgl_probe`。
- 新增 `src/transmitter_s3_lvgl_probe/main.cpp`。
- 选用 `LovyanGFX@1.2.21` 作为第一轮尝试库。
- 按现有 ESP-IDF 工程引脚配置 GC9A01 240x240 8080 8bit LCD。
- 按现有 ESP-IDF 工程引脚配置 CST816 I2C 触摸。
- 探针界面绘制红/绿/蓝/白色块、圆形边框、中心十字和触摸坐标。
- 触摸串口输出增加越界过滤，便于后续校准。
- S3 探针串口使用普通 UART Serial，未启用 Arduino USB CDC。

### 验证

- `pio run -e s3_lvgl_probe`：SUCCESS。
- `pio run -e s3_lvgl_probe -t upload --upload-port COM7`：SUCCESS。
- `pio run -e transmitter -t upload --upload-port COM3`：SUCCESS。
- `pio run -e receiver -t upload --upload-port COM10`：SUCCESS。
- COM7 串口已抓到启动日志和有效触摸坐标，例如 `TOUCH x=1 y=169`、`TOUCH x=238 y=101`。

### 待确认

- 需要人工目视确认屏幕是否显示彩色测试图。
- 触摸坐标可读取，但坐标方向和边缘映射仍需根据实际触摸位置校准。
- 后续高级版遥控器需要重新规划实体摇杆、按钮、档位、CW2015 和蜂鸣器的 ESP32-S3 引脚。

## 2026-06-08 ESP32-S3R8 黑屏排查

### 现象

- `s3_lvgl_probe` 上传后 COM7 能输出启动日志和触摸坐标。
- 屏幕实际为黑屏，没有显示测试色块。

### 排查结论

- 屏幕数据引脚、触摸引脚与原 ESP-IDF 工程一致。
- 原 ESP-IDF 工程中 `LCD_POWER` 只配置为输出，没有拉高；结合黑屏现象，初步推断 GPIO41 的 LCD 电源使能为低有效。
- 当前探针已把 GPIO41 从拉高改为低有效输出，作为单变量验证。

### 新增硬件记录

- 高级版开发板外设 I2C：GPIO18 为 SDA，GPIO19 为 SCL。
- 外设 I2C 上挂载 CW2015、BMP280、LSM6DSLTR、QMC5883L。

### 触摸方向校准

- GPIO41 低有效后，用户确认屏幕已点亮。
- 触摸点上下方向跟手，左右方向相反。
- 当前已只反转触摸 X 坐标：`x = 239 - rawX`。
- 串口输出同时保留 `raw_x` 和校正后的 `x`，便于继续验证。

## 2026-06-08 ESP32-S3R8 外设 I2C 仪表页

### 目标

把 GPIO18/GPIO19 上的 CW2015、BMP280、LSM6DSLTR、QMC5883L 驱动起来，并在 S3 圆屏上显示实时数据。

### 已完成

- `s3_lvgl_probe` 新增 `Wire1`，GPIO18 为 SDA，GPIO19 为 SCL。
- 外设 I2C 频率暂定 100kHz，用于提高早期硬件排查稳定性。
- 新增启动 I2C 扫描，串口打印实际响应地址。
- 新增 CW2015 最小驱动，读取电压和 SOC。
- 新增 BMP280 最小驱动，读取温度、气压和估算海拔。
- 新增 QMC5883L 最小驱动，读取磁场 XYZ 和粗略航向角。
- 新增 LSM6DSLTR 最小驱动，支持常见地址 `0x6A` / `0x6B` 和 WHO_AM_I `0x6A`。
- 屏幕改为 `S3 SENSOR DASH` 仪表页，每 500ms 刷新数据。

### 当前硬件结果

- COM7 扫描到 `0x0D`、`0x62`、`0x76`。
- CW2015、BMP280、QMC5883L 已有有效数据。
- LSM6DSLTR 未在 `0x6A` / `0x6B` 响应；当前屏幕显示 `not found`，串口显示 `LSM:0`。
- CW2015 电压换算已对齐原 C3 遥控器代码，不做 14bit 右移。
- 用户补充确认 LSM6DSLTR 另有 GPIO20 `INT1_LSM` 和 GPIO21 `INT2_LSM` 两个中断脚；该信息不改变当前 I2C 扫描结论。

## 2026-06-09 ESP32-S3R8 正式发射端启动

### 目标

在基础硬件验证完成后，创建正式 S3 高级版发射端项目，把 C3 基础版遥控器功能迁移到 S3 新硬件。

### 已完成

- 新增 PlatformIO 环境 `s3_transmitter`。
- 新增正式源码目录 `src/transmitter_s3/`。
- 保留 `s3_lvgl_probe` 作为硬件探针，不再作为正式业务代码入口。
- S3 正式端复用基础版 `ControlPacket` / `StatusPacket` 协议。
- S3 正式端实现 100Hz 控制包发送。
- S3 正式端实现接收端状态包解析和 500ms 连接超时。
- S3 正式端实现开机摇杆中位校准、三档速度、按钮位、本地低电量报警。
- S3 正式端复用 GC9A01/CST816 屏幕触摸配置。
- S3 正式端显示连接状态、VESC 电压、速度、油门、档位、按钮、本地电池、BMP280 和 QMC5883L。
- 接收端新增 S3 发射端 MAC `48:CA:43:9A:A9:B0`，同时保留 C3 发射端 MAC。

### 当前限制

- S3 实体摇杆、档位开关、按钮、蜂鸣器引脚尚未按最终硬件定稿，当前仅为软件占位。
- 另外两块 C3 设备当前未接入，无法做端到端联调上传。
- LSM6DSLTR 暂时不进入正式功能闭环。

## 2026-06-09 S3 正式端加入 LVGL 8.3.11

### 目标

将正式 `s3_transmitter` 的界面层从 LovyanGFX 直接绘制迁移到 LVGL 8.3.11。LovyanGFX 继续作为已经验证过的屏幕和触摸底层驱动，`s3_lvgl_probe` 继续作为硬件探针项目保留。

### 已完成

- `s3_transmitter` 新增依赖 `lvgl/lvgl@8.3.11`。
- 新增 `include/lv_conf.h`，配置 16-bit 色深、LVGL 内存池和当前仪表盘使用的基础控件。
- `s3_transmitter` 新增 LVGL display flush 回调，使用现有 GC9A01/LovyanGFX 配置推送像素。
- `s3_transmitter` 新增 LVGL touch read 回调，沿用已验证的 CST816 触摸 X 轴反向校正。
- 原 LovyanGFX 直接绘制的状态页迁移为 LVGL 标签和油门条。
- 主循环新增 `lv_tick_inc()` 和 `lv_timer_handler()` 调度。
- 控制包发送、ESP-NOW 接收、传感器读取、蜂鸣器、摇杆中位校准、占位 GPIO 未改变。

### 验证

- `pio run -e s3_transmitter`：SUCCESS。
- `pio run -e receiver`：SUCCESS。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e s3_lvgl_probe`：SUCCESS。
- `pio test -e native`：PASS，12/12。
- `pio device list`：当前仅 COM1 和 COM7，无 COM3 / COM10。
- `pio run -e s3_transmitter -t upload --upload-port COM7`：FAILED，COM7 可见但被系统拒绝访问，错误为 `PermissionError(13, '拒绝访问。')`。

### 当前限制

- 正式 S3 LVGL 固件已成功编译，但尚未烧录到 S3；需要释放 COM7 后重试上传。
- 本轮未进行屏幕目视确认，实际颜色、布局和触摸点跟手情况需要上传成功后验证。

### 后续上传复测

- 用户释放 COM7 占用后，`pio run -e s3_transmitter -t upload --upload-port COM7` 已成功。
- 上传识别到 S3 MAC：`48:ca:43:9a:a9:b0`。
- COM7 串口启动日志正常，程序输出 `ESP32-S3 formal transmitter`。
- 外设 I2C 扫描结果：`0x0D`、`0x62`、`0x76`。
- 当前 C3 接收端未接入，串口状态为 `[LOST]`，符合硬件连接状态。

### 仍需人工确认

- 目视确认 LVGL 页面是否正常显示。
- 触摸测试：确认 LVGL 坐标左右/上下均跟手。

## 2026-06-09 S3 发热和 LVGL 显示撕裂初步修复

### 现象

- 用户反馈：S3 屏幕已点亮，但文字有撕裂感，显示不太正常。
- 用户反馈：芯片发热非常严重，特别烫。

### 初步判断

- 当前 S3 处于 `[LOST]`，即接收端未接入状态。
- 正式端相比探针项目多了 ESP-NOW/WiFi，并且原配置在断联时仍 100Hz 发送控制包。
- 原配置同时使用 WiFi 15dBm、背光 255/255、CPU 默认 240MHz、主循环 `delay(1)`，空载功耗偏高。
- 显示撕裂可能与 LVGL 小区域刷新频率和主循环高占空比有关，仍需目视复测确认。

### 已修改

- S3 正式端 CPU 降为 160MHz。
- 屏幕背光从 255/255 降为 140/255。
- WiFi 发射功率从 15dBm 降为 8.5dBm。
- 控制包发送改为：已连接 100Hz，未连接/搜索状态 20Hz。
- LVGL handler 调度限制为 5ms 周期。
- 主循环空闲延时从 1ms 增加到 5ms。

### 验证

- `pio run -e s3_transmitter`：SUCCESS。
- `pio run -e s3_transmitter -t upload --upload-port COM7`：SUCCESS。
- COM7 串口启动日志确认：`S3 power profile: CPU 160MHz, LCD brightness 140, WiFi TX 8.5dBm`。
- `pio run -e receiver`：SUCCESS。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e s3_lvgl_probe`：SUCCESS。
- `pio test -e native`：PASS，12/12。

### 仍需人工确认

- 通电 3 到 5 分钟后，确认芯片温度是否从“烫手”降到可接受。
- 目视确认文字撕裂是否改善。
- 如果仍然明显发热，应断电并继续检查供电、电流、屏幕背光电路、S3 模组焊接和外设短路风险。

### 显示彩边复测补充

- 根据用户照片，文字问题更像红/青彩边，不是整屏撕裂。
- 已确认 LVGL 未启用 subpixel 字体，当前使用的 Montserrat 字体 `.subpx = LV_FONT_SUBPX_NONE`。
- 初步判断为 8-bit 并口屏 RGB565 字节序与 LVGL 默认输出不匹配。
- `include/lv_conf.h` 已将 `LV_COLOR_16_SWAP` 从 `0` 改为 `1`。
- 已清理并重新编译 `s3_transmitter`，确保 LVGL 按新配置重建。
- `pio run -e s3_transmitter -t upload --upload-port COM7`：SUCCESS。
- COM7 串口启动正常。
- 视觉效果仍需用户拍照或目视确认。

## 2026-06-09 S3 LVGL UI 独立层和触摸显示

### 目标

将正式 S3 发射端的 LVGL 页面代码从 `main.cpp` 拆出，形成接近 SquareLine Studio 使用习惯的 `ui_init()` / `ui_update()` 边界，并加入基础触摸可视化。

### 已完成

- 新增 `src/transmitter_s3/ui/ui.h`。
- 新增 `src/transmitter_s3/ui/ui.cpp`。
- `main.cpp` 保留硬件、通信、传感器、输入、LVGL flush 和 touch driver 回调。
- LVGL 页面对象、标签、油门条、颜色、布局和状态更新迁移到 `ui.cpp`。
- 新增 `S3UiState`，由 `main.cpp` 汇总业务状态后传给 `ui_update()`。
- 新增触摸点显示：
  - 按下时显示洋红色小圆点。
  - 按下时显示 `TOUCH x,y`。
  - 松开时隐藏圆点并显示 `TOUCH --`。

### 验证

- `pio run -e s3_transmitter`：SUCCESS。
- `pio run -e s3_transmitter -t upload --upload-port COM7`：SUCCESS。
- COM7 串口启动日志正常。
- `pio run -e receiver`：SUCCESS。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e s3_lvgl_probe`：SUCCESS。
- `pio test -e native`：PASS，12/12。

### 待人工确认

- 用手触摸屏幕，确认 `TOUCH x,y` 坐标变化。
- 确认洋红色小圆点跟随手指移动。
- 确认松手后圆点隐藏。

## 2026-06-09 S3 触摸跟手一级优化

### 目标

在不引入双核 UI 任务、不大改架构的前提下，先降低 LVGL 触摸链路的等待时间，观察触摸点跟手和芯片温度变化。

### 已修改

- 新增 `include/s3_runtime_config.h`，集中记录 S3 正式端运行时节奏参数。
- `LV_DISP_DEF_REFR_PERIOD` 设置为 16ms，约 60Hz 显示刷新。
- `LV_INDEV_DEF_READ_PERIOD` 设置为 5ms，提高触摸读取频率。
- `lv_timer_handler()` 调度周期从 5ms 改为 2ms。
- 主循环空闲延时从 5ms 改为 2ms。
- 页面业务数据更新仍保持 200ms，避免标签和状态文本无意义高频刷新。

### 验证

- `pio test -e native`：PASS，13/13。
- `pio run -e s3_transmitter`：SUCCESS。
- `pio run -e s3_transmitter -t upload --upload-port COM7`：SUCCESS。
- COM7 启动日志确认正式 S3 程序启动，I2C 扫描到 `0x0D`、`0x62`、`0x76`。
- `pio run -e receiver`：SUCCESS。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e s3_lvgl_probe`：SUCCESS。

### 观察项

- 本次串口日志中 CW2015 读取曾短暂出现 `requestFrom()` 错误，随后状态恢复为 `BAT:OK`；先作为观察项，不归入本次触摸优化问题。
- 仍需人工触摸确认圆点延迟是否改善。
- 仍需通电数分钟确认芯片温度是否保持可接受。

## 2026-06-09 S3 LVGL 显示 DMA

### 目标

直接开启 S3 正式端 LVGL 屏幕刷新 DMA，减少 CPU 在 8-bit 并口屏像素推送上的搬运负担。

### 已修改

- `include/s3_runtime_config.h` 新增 `S3_LVGL_DISPLAY_USE_DMA=1`。
- LVGL flush 从 `display.pushImage()` 改为 `display.pushImageDMA()`。
- DMA 刷新后调用 `display.waitDMA()`，再执行 `lv_disp_flush_ready()`，避免 LVGL 绘图缓冲区被提前复用导致花屏。
- 新增 native 单元测试确认 S3 LVGL 显示 DMA 配置为开启。

### 验证

- TDD 红灯：`pio test -e native` 因缺少 `S3_LVGL_DISPLAY_USE_DMA` 失败。
- `pio test -e native`：PASS，14/14。
- `pio run -e s3_transmitter`：SUCCESS。
- `pio run -e s3_transmitter -t upload --upload-port COM7`：SUCCESS。
- COM7 启动日志确认正式 S3 程序启动，I2C 扫描到 `0x0D`、`0x62`、`0x76`。
- `pio run -e receiver`：SUCCESS。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e s3_lvgl_probe`：SUCCESS。

### 观察项

- DMA 可能改善触摸响应和降低 CPU 搬运像素负担，但芯片温度仍需要实测确认。
- 如果出现花屏、局部残影、重启或触摸变卡，需要优先检查 LVGL buffer 与 DMA 传输时序。

## 2026-06-09 S3 页面增加 FPS 显示

### 目标

在 S3 正式端 LVGL 页面上显示屏幕刷新率/帧率参数，便于后续判断触摸延迟、DMA、刷新周期和页面复杂度之间的关系。

### 已修改

- 新增 `displayFpsForFrameCount()`，按指定时间窗口内完成的 LVGL flush 帧数折算 FPS。
- `S3_DISPLAY_FPS_SAMPLE_INTERVAL_MS` 设置为 1000ms。
- LVGL flush callback 每完成一次屏幕 flush 计数一次。
- `updateDashboard()` 每秒更新一次 `displayFps`。
- 页面底部触摸坐标右侧新增 `FPS n` 小字体显示。

### 验证

- TDD 红灯：`pio test -e native` 因缺少 `displayFpsForFrameCount()` 失败。
- `pio test -e native`：PASS，15/15。
- `pio run -e s3_transmitter`：SUCCESS。
- `pio run -e s3_transmitter -t upload --upload-port COM7`：SUCCESS。
- COM7 启动日志确认正式 S3 程序启动，I2C 扫描到 `0x0D`、`0x62`、`0x76`。
- `pio run -e receiver`：SUCCESS。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e s3_lvgl_probe`：SUCCESS。

### 待人工确认

- 目视确认底部右侧是否显示 `FPS n`。
- 触摸屏幕时确认 `TOUCH x,y` 和 `FPS n` 不互相遮挡。

## 2026-06-09 S3 发射端与接收端联调修复

### 现象

- DMA 已确认开启：`S3_LVGL_DISPLAY_USE_DMA=1`，LVGL flush 使用 `pushImageDMA()` 并在 `waitDMA()` 后释放 LVGL buffer。
- 初次联调时 S3 串口持续显示 `[LOST]`。
- 接收端串口显示 `[FAILSAFE]`，PWM 保持中位 76。
- S3 启动日志曾出现一次 `BROWNOUT_RST`，需继续观察供电稳定性。

### 根因

接收端已经允许 S3 发射端 MAC，但 `StatusPacket` 仍固定回发给 C3 基础发射端 MAC，S3 无法收到状态包，因此页面判定断联。

### 已修改

- 接收端收到合法发射器控制包后，记录最后一个合法发射器 MAC。
- 接收端状态包回发给最后一个合法发射器，而不是固定发给 C3 发射端。
- C3 发射端、S3 发射端、接收端均显式固定 ESP-NOW 信道为 1。
- 新增 `rememberStatusTarget()` 单元测试，覆盖 S3 MAC 被记录为状态回传目标。

### 验证

- `pio test -e native`：PASS，16/16。
- `pio run -e receiver`：SUCCESS。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e s3_transmitter`：SUCCESS。
- `pio run -e s3_lvgl_probe`：SUCCESS。
- `pio run -e receiver -t upload --upload-port COM10`：SUCCESS。
- `pio run -e s3_transmitter -t upload --upload-port COM7`：SUCCESS。
- COM7/COM10 20 秒联调：
  - S3 连续输出 `[OK]`。
  - 接收端持续输出 `THR:   0 SPD:1 BTN:00`。
  - 接收端 PWM 中位为 76。
  - 未再进入 `[FAILSAFE]`。

## 2026-06-09 S3 SquareLine Studio 工程接入

### 目标

把 SquareLine Studio 原始工程和导出的空页面纳入当前仓库，并让导出的 LVGL 代码可以和现有 S3 正式发射端业务 UI 共存。

### 已修改

- 新增 `.gitignore` 规则，忽略 SquareLine `cache/`、`backup/`、临时文件和系统噪声文件。
- SquareLine 原始工程路径：`tools/ui_projects/s3_transmitter_squareline/`。
- SquareLine 导出代码路径：`src/transmitter_s3/ui/generated/`。
- 手写业务 UI 适配层函数改为 `s3_ui_init()`、`s3_ui_update()`、`s3_ui_set_touch()`，避免和 SquareLine 生成的 `ui_init()` 冲突。
- `s3_ui_init()` 先调用 SquareLine 生成的 `ui_init()`，再叠加当前遥控器状态、触摸点和 FPS 显示。
- 新增 `tools/platformio/patch_squareline_export.py`，在 `s3_transmitter` 编译前自动屏蔽 SquareLine 生成文件中与实机颜色配置冲突的 `LV_COLOR_16_SWAP == 0` 检查。
- 新增 `docs/ui-workflow.md` 记录导出和集成流程。

### 验证

- `pio test -e native`：PASS，16/16。
- `pio run -e s3_transmitter`：SUCCESS。
- `pio run -e s3_transmitter -t upload --upload-port COM7`：SUCCESS。
- COM7 启动日志确认正式 S3 程序启动，I2C 扫描到 `0x0D`、`0x62`、`0x76`，连接状态为 `[OK]`。

### 后续规则

- 不手动修改 `src/transmitter_s3/ui/generated/` 中的业务逻辑。
- 重新导出后先运行 `pio run -e s3_transmitter`，自动补丁会处理当前已知的 SquareLine 颜色检查冲突。
- 需要持久使用的控件名称应在 SquareLine 中命名清楚，再由 `s3_ui_update()` 引用。

## 2026-06-09 S3 SquareLine 背景图页面导出

### 目标

验证 SquareLine Studio 中新增的 Screen 和背景图资源可以直接导出、编译并上传到 S3 正式发射端。

### 已修改

- SquareLine 导出新增 `src/transmitter_s3/ui/generated/ui_img_bg3_png.c`。
- `ui_Screen1.c` 使用 `lv_img_create()` 和 `lv_img_set_src()` 加载背景图。
- SquareLine 原始工程保留 `assets/bg3.png`。
- `.gitignore` 增加 `autosave/` 和 `project.info`，避免提交本地自动保存包和本机元数据。

### 验证

- `pio run -e s3_transmitter`：SUCCESS。
- `pio run -e s3_transmitter -t upload --upload-port COM7`：SUCCESS。
- COM7 短时监听未捕获到启动日志；屏幕视觉效果需以实物观察为准。

## 2026-06-09 S3 SquareLine 图片颜色异常修复

### 现象

SquareLine Studio 预览中背景图为深色，但实机显示为粉色/绿色偏色；同屏文字颜色基本正常。

### 判断

问题集中在 SquareLine 导出的 `LV_IMG_CF_TRUE_COLOR` 图片资源。项目使用 `LV_COLOR_DEPTH=16` 和 `LV_COLOR_16_SWAP=1`，而 SquareLine 导出的 true color 图片数据未按当前字节序转换，导致图片颜色异常。

### 已修改

- 增强 `tools/platformio/patch_squareline_export.py`。
- 构建前自动处理 `src/transmitter_s3/ui/generated/ui_img_*.c` 中的 `LV_IMG_CF_TRUE_COLOR` 图片资源。
- 对 16-bit 图片数据执行字节对调，并写入 marker，避免重复转换。

### 验证

- `pio run -e s3_transmitter`：SUCCESS。
- `pio test -e native`：PASS，16/16。
- 初次上传失败原因是残留 `pio device monitor -p COM7 -b 115200` 占用串口。
- 结束残留 monitor 后，`pio run -e s3_transmitter -t upload --upload-port COM7`：SUCCESS。
- 实机颜色效果仍需目视确认。

## 2026-06-09 S3 SquareLine 正规数据绑定

### 目标

把 S3 页面改为常规 SquareLine 集成方式：SquareLine 负责控件和静态样式，固件代码负责数据绑定。

### 已修改

- 新增 `include/s3_ui_bindings.h`，集中处理 UI 数值转换。
- 新增 `s3BatteryPercentForArc()`，将 CW2015 SOC 转换为 Arc 所需的 `0-100` 整数值。
- `s3_ui_update()` 已把当前 SquareLine 导出的 `ui_BAT` 绑定到 `S3UiState::cw2015Soc`。
- 电池 Arc 在无效电量时显示 `0`，低于等于 20% 时指示色变红。
- 新增 `docs/squareline-data-binding.md`，记录 SquareLine 控件命名、数据来源和绑定方式。

### 验证

- 新增 native 单测 `test_s3_battery_arc_value_clamps_and_rounds_soc`。
- `pio test -e native`：PASS，17/17。
- `pio run -e s3_transmitter`：SUCCESS。
- `pio run -e s3_transmitter -t upload --upload-port COM7`：SUCCESS。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e receiver`：SUCCESS。

## 2026-06-09 S3 SquareLine 控件名编译错误修复

### 现象

`pio run -e s3_transmitter` 编译失败，`src/transmitter_s3/ui/ui.cpp` 报 `ui_ArcBattery was not declared in this scope`。

### 原因

SquareLine 重新导出后，电池 Arc 的实际实例名是 `ui_uiArcBattery`，电量文字 Label 是 `ui_uiLabelBattery`；而手写绑定层引用了推荐名 `ui_ArcBattery`。

### 已修改

- `src/transmitter_s3/ui/ui.cpp` 改为绑定实际导出的 `ui_uiArcBattery`。
- 同步绑定 `ui_uiLabelBattery`，显示电量百分比或 `--%`。
- 更新 `docs/squareline-data-binding.md`，说明编译错误时以 `ui_Screen1.h` 中的实际对象名为准。

### 验证

- `pio run -e s3_transmitter`：SUCCESS。
- `pio test -e native`：PASS，17/17。
- `pio run -e s3_transmitter -t upload --upload-port COM7`：SUCCESS。

## 2026-06-09 S3 速度 Label 数据绑定和颜色规则

### 目标

把 SquareLine 中新增的 `uiLabelSpeed` 接入真实轮速数据，并按速度区间显示不同文字颜色。

### 已修改

- 在 `include/s3_ui_bindings.h` 新增 `s3SpeedColorHex()`。
- 速度颜色规则：`0-14 km/h` 绿色，`15-29 km/h` 黄色，`>=30 km/h` 红色。
- 在 `src/transmitter_s3/ui/ui.cpp` 中绑定实际导出的 `ui_uiLabelSpeed`。
- `ui_uiLabelSpeed` 显示 `S3UiState::receiverSpeed` 数值，并按 `s3SpeedColorHex()` 更新文字颜色。
- 更新 `docs/squareline-data-binding.md`，记录速度 Label 的绑定方式。

### 验证

- 新增 native 单测 `test_s3_speed_label_color_uses_speed_bands`。
- `pio test -e native`：PASS，18/18。
- `pio run -e s3_transmitter`：SUCCESS。

## 2026-06-09 S3 状态 Label 数据绑定

### 目标

接入 SquareLine 中新增的 `ui_LabelStatus`，显示接收器连接状态和 VESC 回传电压。

### 已修改

- SquareLine 生成控件命名已修正为 `ui_ArcBattery`、`ui_LabelBattery`、`ui_LabelSpeed`、`ui_LabelStatus`。
- `src/transmitter_s3/ui/ui.cpp` 同步绑定新的正式控件名。
- `ui_LabelStatus` 在连接时显示 `OK xx.xxV`，断联时显示 `LOST`。
- `ui_LabelStatus` 在连接时显示绿色，断联时显示红色。
- 更新 `docs/squareline-data-binding.md` 中的实际控件名和状态 Label 说明。

### 验证

- `pio run -e s3_transmitter`：SUCCESS。
- `pio test -e native`：PASS，18/18。
- `pio run -e s3_transmitter -t upload --upload-port COM7`：SUCCESS。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e receiver`：SUCCESS。
- `git diff --check`：PASS，仅有 CRLF 换行提示。

## 2026-06-09 S3 控制/BMP280/VESC 电压控件绑定

### 目标

接入 SquareLine 中新增的 `ui_LabelControl`、`ui_LabelBmp280`、`ui_BarThrottle`、`ui_LabelStatusVoltage`、`ui_ArcStatusVoltage`。

### 已修改

- `ui_LabelControl` 显示当前档位，格式为 `SPD n`。
- `ui_BarThrottle` 设置为 `-1000` 到 `1000` 的对称 Bar，摇杆回中时保持在中位。
- 油门正值向右增长并显示绿色，刹车负值向左增长并显示橙色，回中显示灰色。
- `ui_ArcStatusVoltage` 显示 VESC 电压，范围固定为 `6-48V`；断联时回到最低值。
- `ui_LabelStatusVoltage` 连接时显示 `xx.xxV`，断联时显示 `--.-V`。
- `ui_LabelBmp280` 显示气压和估算海拔，格式为 `xxxxhPa xxxm`。
- `S3UiState` 新增 `bmp280AltitudeM`，正式 S3 主程序读取 BMP280 后计算海拔并传入 UI。
- `include/s3_ui_bindings.h` 新增 VESC 电压、油门 Bar、油门颜色和 BMP280 海拔的映射函数。
- 更新 `docs/squareline-data-binding.md` 中的新增控件绑定说明。

### 验证

- 新增 native 单测：
  - `test_s3_voltage_arc_value_clamps_to_vesc_display_range`
  - `test_s3_throttle_bar_keeps_center_and_uses_direction_colors`
  - `test_s3_bmp280_altitude_uses_standard_sea_level_pressure`
- `pio test -e native`：PASS，21/21。
- `pio run -e s3_transmitter`：SUCCESS。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e receiver`：SUCCESS。
- `pio run -e s3_transmitter -t upload --upload-port COM7`：SUCCESS。
- `git diff --check`：PASS，仅有 CRLF 换行提示。

## 2026-06-09 S3 旧手写参数层清理

### 目标

SquareLine 页面已经承接主要参数显示后，删除 `src/transmitter_s3/ui/ui.cpp` 中原来直接创建的调试/占位参数文字层，避免同一数据在屏幕上重复渲染。

### 已修改

- 删除手写创建的标题、连接状态、档位/油门、速度、电池、BMP280、航向、按钮/RSSI、FPS、触摸坐标和占位 GPIO 提示。
- 删除旧手写油门 Bar，只保留 SquareLine 导出的 `ui_BarThrottle`。
- `s3_ui_update()` 现在只更新 SquareLine 导出的正式控件。
- `s3_ui_set_touch()` 暂时改为空实现；后续若需要显示触摸坐标/触摸点，应先在 SquareLine 中添加 `ui_LabelTouch` / `ui_DotTouch` 后再绑定。
- 更新 `docs/squareline-data-binding.md`，记录旧手写层已删除，以及 FPS/触摸控件仍需 SquareLine 页面提供。

### 验证

- `pio run -e s3_transmitter`：SUCCESS。
- `pio test -e native`：PASS，21/21。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e receiver`：SUCCESS。
- `pio run -e s3_transmitter -t upload --upload-port COM7`：SUCCESS。
- `git diff --check`：PASS，仅有 CRLF 换行提示。

## 2026-06-09 双版本发射端上电解锁/油门斜率/摇杆设置页

### 目标

为 C3 OLED 基础版和 S3 触摸高级版同步加入上电解锁、油门变化率限制和摇杆中位设置入口。按钮 1 改为本地设置页面入口；S3 版本额外提供触摸校准和中位微调。

### 已修改

- `include/control_logic.h` 新增：
  - `canArmTransmitter()`：只有摇杆回到安全阈值内才允许解锁。
  - `safeThrottleForArming()`：未解锁时强制 throttle 为 0。
  - `slewLimitedThrottle()`：限制 throttle 单次变化步进。
- C3 `transmitter`：
  - 上电后必须摇杆回中才会解锁；未解锁前发送 throttle 为 0。
  - 发送 throttle 经过斜率限制，避免瞬间大油门。
  - 按钮 1 进入/退出 OLED 设置页面，按钮 1 不再作为控制包 bit0 上报。
  - 设置页面内输出锁定为 0；按钮 2 执行摇杆中位校准。
- S3 `s3_transmitter`：
  - 同步加入上电解锁和 throttle 斜率限制。
  - 按钮 1 进入/退出触摸设置覆盖层，按钮 1 不再作为控制包 bit0 上报。
  - 设置页面内输出锁定为 0。
  - 触摸 `CAL` 重新采样中位，触摸 `-10` / `+10` 微调中位，触摸 `EXIT` 退出。
  - 未解锁时 `ui_LabelStatus` 显示 `LOCK`，颜色为黄色。
- 更新 `docs/user-guide.md`、`docs/hardware.md`、`docs/squareline-data-binding.md`。

### 验证

- 新增 native 单测：
  - `test_transmitter_arming_requires_centered_joystick`
  - `test_transmitter_safety_forces_zero_until_armed`
  - `test_throttle_slew_rate_limits_step_changes`
- 初次运行 `pio test -e native` 因 helper 未实现失败，随后实现后通过。
- `pio test -e native`：PASS，24/24。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e s3_transmitter`：SUCCESS。
- `pio run -e receiver`：SUCCESS。
- `git diff --check`：PASS，仅有 CRLF 换行提示。
- `pio run -e transmitter -t upload --upload-port COM3`：SUCCESS。
- `pio run -e s3_transmitter -t upload --upload-port COM7`：SUCCESS。
- 实物 UI 和触摸设置页操作仍需人工观察确认。

## 2026-06-09 解锁逻辑修正和 S3 BMP 显示修复

### 现象

- 上一版误做成“摇杆回中自动解锁”，实际需求是“刹车拉满 3 秒解锁”。
- S3 删除旧手写参数层后，BMP 参数只剩 SquareLine 的 `ui_LabelBmp280`，该 Label 没有显式文字样式，可能出现看不清或文字过长挤压。

### 已修改

- `include/control_logic.h` 新增 `shouldArmByBrakeHold()`。
- C3 和 S3 发射端均改为：摇杆拉到最大刹车方向并保持 3 秒后解锁。
- 松开刹车或进入/退出设置页会重置解锁保持计时。
- S3 `ui_LabelBmp280` 在绑定层强制设置白色、12px 字体、居中对齐。
- S3 BMP 显示改为两行：`xxxxhPa` + `xxxm`。
- 更新 `docs/user-guide.md` 和 `docs/squareline-data-binding.md`。

### 验证

- 新增/调整 native 单测：
  - `test_transmitter_arming_requires_full_brake_hold_for_three_seconds`
  - `test_transmitter_brake_hold_resets_when_brake_released`
- 初次运行 `pio test -e native` 因 `shouldArmByBrakeHold()` 未实现失败，随后实现后通过。
- `pio test -e native`：PASS，25/25。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e s3_transmitter`：SUCCESS。
- `pio run -e receiver`：SUCCESS。
- `git diff --check`：PASS，仅有 CRLF 换行提示。
- `pio run -e transmitter -t upload --upload-port COM3`：SUCCESS。
- `pio run -e s3_transmitter -t upload --upload-port COM7`：SUCCESS。
- S3 BMP 参数的实物显示仍需上电后人工确认。

## 2026-06-09 S3 状态 Label 解锁标记显示调整

### 现象

S3 未解锁时 `ui_LabelStatus` 被直接写成 `LOCK`，导致原本的连接状态 `OK` / `LOST` 被覆盖。

### 已修改

- `include/s3_ui_bindings.h` 新增 `s3FormatStatusText()`。
- `src/transmitter_s3/ui/ui.cpp` 改为先保留连接状态，再在未解锁时追加 `LOCK`。
- 显示格式改为：`OK`、`LOST`、`OK LOCK`、`LOST LOCK`。
- 更新 `docs/squareline-data-binding.md`。

### 验证

- 新增 native 单测 `test_s3_status_text_appends_lock_without_hiding_link_status`。
- `pio test -e native`：PASS，26/26。
- `pio run -e s3_transmitter`：SUCCESS。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e receiver`：SUCCESS。
- `git diff --check`：PASS，仅有 CRLF 换行提示。
- `pio run -e s3_transmitter -t upload --upload-port COM7`：SUCCESS。

## 2026-06-09 S3 间歇性断联稳定性调整

### 现象

S3 高级发射端使用过程中会间歇性显示或表现为断联。

### 分析

- S3 原 ESP-NOW 发射功率设置为 `8.5dBm`，对实际遥控链路偏保守，边缘信号下更容易丢包。
- S3 页面状态包超时原为 `500ms`；接收端遥测理论为 `20Hz`，但 LCD 刷新、I2C 读取、串口输出或无线重传造成短暂抖动时，UI 容易提前显示 `LOST`。
- 接收端 failsafe 超时仍为 `2000ms`，本次没有放宽接收端安全保护。

### 已修改

- `include/s3_runtime_config.h` 新增：
  - `S3_ESPNOW_STATUS_TIMEOUT_MS = 1000`
  - `S3_ESPNOW_TX_POWER_DBM_X4 = 78`
- S3 状态回包超时从 `500ms` 调整到 `1000ms`。
- S3 WiFi/ESP-NOW 发射功率从 `8.5dBm` 调整到 `19.5dBm`。
- S3 启动日志同步显示当前 WiFi TX dBm。

### 验证

- 新增 native 单测 `test_s3_espnow_link_uses_stable_radio_profile`。
- 初次运行 `pio test -e native` 因新配置未实现失败，随后实现后通过。
- `pio test -e native`：PASS，29/29。
- `pio run -e s3_transmitter`：SUCCESS。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e receiver`：SUCCESS。
- `pio run -e s3_transmitter -t upload --upload-port COM7`：SUCCESS。
- 串口抓取尝试：COM7/COM10 均被占用，未能完成 15 秒在线日志确认。

## 2026-06-09 S3 电量/BMP280 瞬时归零修复

### 现象

S3 页面上的本机电量和 BMP280 气压/海拔偶发变成 `0` 或无效显示，随后又瞬间恢复。

### 原因

`readCw2015()` 和 `readBmp280()` 在每次读取开始时直接清空 `valid`。当 AUX I2C 出现一次短暂读取失败，或读到全 0 / `NaN` / 越界坏帧时，UI 会立即拿到无效或 0 值，导致 Arc/Label 闪一下。

### 已修改

- `include/s3_ui_bindings.h` 新增：
  - `s3Cw2015ReadingIsPlausible()`
  - `s3Bmp280ReadingIsPlausible()`
- S3 读取层改为保留最后一次可信 CW2015/BMP280 数据。
- 单次或少量连续读取失败不会清空显示值；连续失败 6 次才标记为无效。
- CW2015 过滤 `0V/0%`、`NaN`、SOC 越界等不可信读数。
- BMP280 过滤 `0hPa/0m`、`NaN`、气压/海拔越界等不可信读数。
- 更新 `docs/squareline-data-binding.md`。

### 验证

- 新增 native 单测：
  - `test_s3_cw2015_reading_rejects_transient_zero_or_nan_values`
  - `test_s3_bmp280_reading_rejects_transient_zero_or_nan_values`
- 初次运行 `pio test -e native` 因新 helper 未实现失败，随后实现后通过。
- `pio test -e native`：PASS，28/28。
- `pio run -e s3_transmitter`：SUCCESS。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e receiver`：第一次并行运行遇到 PlatformIO cache 权限/锁提示，单独重跑 SUCCESS。
- `git diff --check`：PASS，仅有 CRLF 换行提示。
- `pio run -e s3_transmitter -t upload --upload-port COM7`：SUCCESS。
