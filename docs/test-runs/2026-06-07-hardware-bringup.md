# 2026-06-07 Hardware Bring-Up Test Run

## 1. 测试环境

| 项目 | 值 |
|---|---|
| 发射端端口 | COM3 |
| 发射端 MAC | `AC:EB:E6:44:D5:54` |
| 接收端端口 | COM10 |
| 接收端 MAC | `AC:EB:E6:44:C5:90` |
| 开发板 | ESP32-C3-DevKitM-1 |
| framework | Arduino |
| 测试方式 | PlatformIO 编译/上传 + Python pyserial 抓取串口日志 |

## 2. 构建和上传

| 项目 | 命令 | 结果 |
|---|---|---|
| 发射端编译 | `pio run -e transmitter` | PASS |
| 接收端编译 | `pio run -e receiver` | PASS |
| 发射端上传 | `pio run -e transmitter -t upload --upload-port COM3` | PASS |
| 接收端上传 | `pio run -e receiver -t upload --upload-port COM10` | PASS |

## 3. 运行时配置修正

首次上传后只能看到 ROM/ESP-IDF 日志，看不到应用层 `Serial.print` 输出。排查本地 PlatformIO 板卡配置后确认 `esp32-c3-devkitm-1` 默认未开启 USB CDC。

已在 `platformio.ini` 增加：

```ini
build_flags =
  -DARDUINO_USB_MODE=1
  -DARDUINO_USB_CDC_ON_BOOT=1
```

重新编译和上传后，COM3/COM10 均能看到应用层启动日志。

## 4. 已验证项目

| 编号 | 测试项 | 结果 | 证据 |
|---|---|---|---|
| BUILD-01 | 发射端编译 | PASS | PlatformIO 返回 `transmitter SUCCESS` |
| BUILD-02 | 接收端编译 | PASS | PlatformIO 返回 `receiver SUCCESS` |
| UPLOAD-01 | 发射端上传到 COM3 | PASS | esptool 识别 MAC `ac:eb:e6:44:d5:54` 并完成写入 |
| UPLOAD-02 | 接收端上传到 COM10 | PASS | esptool 识别 MAC `ac:eb:e6:44:c5:90` 并完成写入 |
| LINK-01 | ESP-NOW 配对 | PASS | 发射端显示 `[OK]`；接收端显示绑定发射器 MAC |
| TX-01 | OLED 初始化 | PASS | 发射端日志 `OLED初始化完成` |
| TX-02 | CW2015 初始化 | PASS | 发射端日志 `CW2015初始化成功`，初始电量约 `4.23V 100%` |
| TX-03 | 摇杆正向 | PASS | 发射端日志捕获 `THR: 999` |
| TX-04 | 摇杆反向 | PASS | 发射端日志捕获 `THR:-1000` |
| RX-01 | PWM 输出响应 | PASS | 接收端 PWM duty 随摇杆从 `89` 到 `63` 再回 `77` |
| BTN-02 | 按钮 2 协议传输 | PASS | 发射端和接收端均捕获 `BTN:02` |
| VESC-01 | VESC 读取未报错 | PASS | 长时间日志未出现 `读取 VESC 失败` |

## 5. 未完成或未确认项目

| 编号 | 测试项 | 当前状态 | 说明 |
|---|---|---|---|
| SPD-01 | 档位 1 | 未确认 | 日志未捕获 `SPD:1` 的动态切换，只在启动/默认状态曾出现 |
| SPD-02 | 档位 2 | 未确认 | 日志未捕获 `SPD:2` |
| SPD-03 | 档位 3 | PASS | 日志多次捕获 `SPD:3` |
| BTN-01 | 按钮 1 | 未确认 | 日志未捕获 `BTN:01` |
| FS-01 | 接收端失控保护 | 未完成 | 测试期间接收端仍持续收到发射端数据，未进入 failsafe |
| OLED-02 | OLED 内容目视检查 | 未确认 | 串口可确认初始化，显示内容需要目视确认 |
| BUZZER-01 | 蜂鸣器声音 | 未确认 | 串口无法直接证明声音，需要目视/听觉确认 |

## 6. 发现的问题

| ID | 问题 | 处理 |
|---|---|---|
| BUG-006 | PlatformIO 下 ESP32-C3 应用层串口日志不可见 | 已通过 USB CDC build flags 修正 |
| BUG-007 | 发射端启动时出现一次 LEDC warning | 记录观察，暂未处理 |
| BUG-004 | 接收端串口打印 `smoothedThrottle`，但该变量未更新 | 记录观察，PWM duty 可证明实际控制链路有效 |

## 7. 下一轮建议

1. 手动确认 OLED 页面内容和蜂鸣器声音。
2. 单独测试三档开关：每档停留 3 秒以上，确认 `SPD:1/2/3` 均出现。
3. 单独测试按钮 1：按下保持 1 秒，确认 `BTN:01`。
4. 做失控保护测试时，让发射端真正断电或拔掉发射端供电，不只是打开串口。
5. 后续代码整理时修正接收端调试打印，让日志显示实际 `throttle` 而不是未更新的 `smoothedThrottle`。
