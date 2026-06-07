# Bug And Maintenance Log

## 状态说明

| 状态 | 含义 |
|---|---|
| Open | 未处理 |
| In Progress | 正在处理 |
| Fixed | 已修复 |
| Won't Fix | 明确不处理 |
| Watch | 暂不处理，但需观察 |

## 维护项

| ID | 状态 | 严重度 | 标题 | 说明 | 验证方式 |
|---|---|---|---|---|---|
| BUG-001 | Fixed | Medium | 发射端摇杆中心固定为 2048 | 已改为上电采样 64 次并取平均值作为校准中心，避免摇杆实际中位偏差导致零点漂移 | 单元测试覆盖校准平均值；上板后需确认回中 throttle 接近 0 |
| BUG-002 | Watch | Low | `StatusPacket` 注释长度不准确 | 字段实际长度为 14 字节，旧注释写 10 字节 | 协议文档已记录实际长度 |
| BUG-003 | Watch | Low | `StatusPacket.motorPWM[4]` 冗余 | 字段暂时保留；状态包已初始化清零，并写入当前 PWM 估算值到 `motorPWM[0]` | 删除后两端通信仍正常 |
| BUG-004 | Fixed | Low | 接收端 `smoothedThrottle` 当前未实际更新 | 串口打印已改为实际限幅后的 throttle，避免调试输出误导 | 串口打印应显示实际控制值 |
| BUG-005 | Watch | Medium | ESP-NOW 初始化失败仅打印日志 | 当前可运行基线保留该行为；后续可考虑进入错误状态 | 模拟初始化失败时应有明确报警或停机策略 |
| BUG-006 | Fixed | Medium | PlatformIO 下 ESP32-C3 应用层串口日志不可见 | 原因是 `esp32-c3-devkitm-1` 板卡配置未默认启用 USB CDC；已在 `platformio.ini` 增加 `ARDUINO_USB_MODE=1` 和 `ARDUINO_USB_CDC_ON_BOOT=1` | 重新上传后 COM3/COM10 能看到应用层启动日志 |
| BUG-007 | Watch | Low | 发射端启动时出现一次 LEDC warning | 日志为 `ledc_get_duty(...): LEDC is not initialized`，出现在启动提示音附近；目前未阻止系统运行 | 后续检查 `tone()` 在 ESP32-C3 上的初始化行为 |
| BUG-008 | Fixed | Medium | 接收端失控蜂鸣可能被按钮逻辑立即打断 | `checkFailsafe()` 启动 1000ms 蜂鸣后，按钮 2 逻辑不再立即 `noTone()` 打断该提示音 | 上板断开发射端，确认接收端失控提示音持续约 1 秒 |
| BUG-009 | Fixed | High | 遥控器断电后接收端可能继续使用断联前控制状态 | 根因是 failsafe 只清零 throttle，没有统一清理 speedLevel、buttons、connected；已新增 `applyReceiverFailsafe()`，进入 failsafe 后清为 throttle=0、speedLevel=1、buttons=0、connected=false、failsafeActive=true | 单元测试覆盖；硬件断电复测待完成 |

## 新 bug 记录模板

```text
ID:
状态:
严重度:
发现日期:
影响端: transmitter / receiver / both
现象:
复现步骤:
期望行为:
实际行为:
初步判断:
修复记录:
验证方式:
```
