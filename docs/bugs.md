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
| BUG-010 | Fixed | Medium | 遥控器未打开或失效时接收端缺少周期提示音 | 已新增接收端链路异常提示：未连接或 failsafe 状态下每 2 秒蜂鸣 200ms；连接正常时静默 | 单元测试覆盖节流逻辑；在线静默已验证；手动关闭发射器后接收端短促间隔滴滴声已验证 |
| BUG-011 | Fixed | High | 接收端偶发长鸣 | 根因判断：按钮 2 远程蜂鸣使用无时长 `tone(BUZZER_PIN, 2000)`，一旦接收到异常或卡住的 `BTN:02` 状态，蜂鸣器会持续响；已增加远程蜂鸣单次最长 3000ms 的超时保护，松开按钮后可重新触发 | 单元测试覆盖远程蜂鸣 3 秒上限和松开复位；已编译并上传新固件；COM10 串口已验证 `BTN:02` 后触发超时保护日志；长时间实机观察仍建议继续记录 |
| BUG-012 | Watch | High | S3 正式端 LVGL 页面文字彩边且芯片严重发热 | 发热初步根因判断：正式端在接收端未接入时仍 100Hz ESP-NOW 空发、WiFi 15dBm、背光满亮、主循环 1ms 空转，和探针项目相比功耗显著更高；已改为断联 20Hz 搜索、已连接 100Hz 控制、CPU 160MHz、背光 140/255、LVGL handler 5ms、主循环 delay 5ms。文字彩边初步根因判断：8-bit 并口屏的 RGB565 字节序与 LVGL 默认输出不匹配；已设置 `LV_COLOR_16_SWAP=1`。后续因 BUG-014 间歇断联，WiFi TX 已重新提高到 19.5dBm | 已编译、上传 COM7；RGB565 字节交换固件已上传；仍需用户触摸温度复测和目视确认文字彩边是否改善 |
| BUG-013 | Fixed | High | S3 发射端接收不到接收器状态包 | 接收端虽然接受 S3 MAC，但状态包仍固定回发到 C3 发射端 MAC，导致 S3 页面持续 `[LOST]`；已改为记录最后一个合法发射器 MAC，并把 `StatusPacket` 回发给该目标；三端 ESP-NOW peer 信道显式固定为 1 | 单元测试覆盖状态目标 MAC 记录；COM7/COM10 上传后 20 秒联调确认 S3 连续 `[OK]`，接收端退出 `[FAILSAFE]`，PWM 中位 76 |
| BUG-014 | Watch | High | S3 间歇性断联 | 初步判断链路余量不足和 S3 状态回包超时过紧共同导致：S3 WiFi TX 原为 8.5dBm，页面状态回包超时原为 500ms；已提高到 19.5dBm，并将 S3 状态回包超时放宽到 1000ms。接收端 failsafe 仍为 2000ms | native 单测覆盖链路配置；三端编译通过；COM7 已上传。串口在线确认因 COM7/COM10 被占用未完成，需实物观察是否仍出现 LOST/failsafe |

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
