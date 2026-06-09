# 2026-06-09 ESP32-S3 Formal Transmitter

## 目标

基础 S3 硬件探针验证完成后，创建正式 `s3_transmitter` 环境，把 C3 基础版遥控器功能迁移到 ESP32-S3R8 触摸屏硬件。

## 本次实现

- 新增 PlatformIO 环境 `s3_transmitter`。
- 新增 `src/transmitter_s3/main.cpp`。
- 保留 `s3_lvgl_probe` 作为硬件探针。
- S3 正式端复用基础版 ESP-NOW 协议。
- S3 正式端实现：
  - 100Hz 控制包发送。
  - 接收端状态包解析。
  - 500ms 连接超时。
  - 开机摇杆中位校准。
  - 三档速度。
  - 按钮状态。
  - 本地 CW2015 电池。
  - BMP280 / QMC5883L 辅助显示。
  - GC9A01 圆屏状态页。
- 接收端兼容 C3 发射器 MAC 和 S3 发射器 MAC。

## 占位引脚

实体硬件尚未画板，下列引脚仅用于让软件结构先跑通：

| 功能 | GPIO |
|---|---|
| 摇杆 ADC | GPIO1 |
| 档位 1 | GPIO37 |
| 档位 2 | GPIO38 |
| 档位 3 | GPIO39 |
| 按钮 1 | GPIO35 |
| 按钮 2 | GPIO36 |
| 蜂鸣器 | GPIO42 |

## 待验证

| 项目 | 结果 |
|---|---|
| `pio run -e s3_transmitter` | SUCCESS |
| `pio run -e receiver` | SUCCESS |
| `pio run -e transmitter` | SUCCESS |
| `pio run -e s3_lvgl_probe` | SUCCESS |
| `pio test -e native` | PASS，12/12 |
| `pio run -e s3_transmitter -t upload --upload-port COM7` | FAILED，COM7 被占用或不可打开 |
| `pio device list` | 当前只有 COM1 和 COM7，没有 COM3 / COM10 |
| COM7 启动日志和屏幕显示 | 未完成，因 COM7 上传失败 |

## 已知限制

- C3 发射器和接收器已被用户拔掉，本轮可能无法上传 COM3 / COM10。
- LSM6DSLTR 暂不纳入正式功能闭环。
- 本轮 `s3_transmitter` 固件已成功编译，但尚未烧录到 S3；需要释放 COM7 后重新上传。

## LVGL 8.3.11 集成复测

### 本次变更

- 正式 `s3_transmitter` 加入 `lvgl/lvgl@8.3.11`。
- 新增 `include/lv_conf.h`，并通过 `-DLV_CONF_INCLUDE_SIMPLE` 与 `-I include` 让 LVGL 库编译阶段读取项目配置。
- LovyanGFX 继续负责 GC9A01 屏幕和 CST816 触摸底层驱动。
- 新增 LVGL flush 回调、触摸读取回调、仪表盘标签和油门条。
- `s3_lvgl_probe` 未加入 LVGL，仍作为 LovyanGFX 硬件探针保留。

### 验证结果

| 项目 | 结果 |
|---|---|
| `pio run -e s3_transmitter` | SUCCESS，RAM 40.3%，Flash 29.8% |
| `pio run -e receiver` | SUCCESS |
| `pio run -e transmitter` | SUCCESS |
| `pio run -e s3_lvgl_probe` | SUCCESS |
| `pio test -e native` | PASS，12/12 |
| `pio device list` | COM1、COM7；无 COM3 / COM10 |
| `pio run -e s3_transmitter -t upload --upload-port COM7` | 初次 FAILED，COM7 可见但被系统拒绝访问：`PermissionError(13, '拒绝访问。')` |

### 待硬件确认

- 释放 COM7 后重新上传正式 `s3_transmitter` 固件。
- 目视确认 LVGL 页面是否正常显示。
- 触摸测试：确认 LVGL 坐标仍然左右/上下跟手。
- 如颜色红蓝异常，再检查 `LV_COLOR_16_SWAP` 或 LovyanGFX 像素推送格式。

## COM7 释放后上传复测

### 执行结果

| 项目 | 结果 |
|---|---|
| `pio device list` | COM1、COM7 |
| `pio run -e s3_transmitter -t upload --upload-port COM7` | SUCCESS |
| 上传识别芯片 | ESP32-S3 revision v0.2，8MB PSRAM |
| 上传识别 MAC | `48:ca:43:9a:a9:b0` |
| 固件大小 | RAM 40.3%，Flash 29.8% |
| COM7 串口读取 | SUCCESS |

### 串口关键信息

```text
ESP32-S3 formal transmitter
S3 joystick center: 58
S3 AUX I2C scan
  found 0x0D
  found 0x62
  found 0x76
S3 transmitter MAC: 48:CA:43:9A:A9:B0
S3 THR:0 SPD:1 BTN:00 [LOST] BAT:OK
```

### 结论

- 正式 `s3_transmitter` LVGL 固件已成功烧录到 S3。
- 程序启动、摇杆中位校准、外设 I2C 扫描、CW2015 状态读取、串口状态输出均正常。
- 当前 `[LOST]` 是因为 C3 接收端未接入，符合本轮硬件连接状态。
- 仍需人工目视确认屏幕 LVGL 页面和触摸方向。

## S3 发热和 LVGL 显示撕裂复测

### 用户反馈

- 屏幕已点亮。
- 屏幕文字有撕裂感，不太正常。
- 芯片发热非常严重，特别烫。

### 本次软件调整

| 参数 | 调整前 | 调整后 |
|---|---:|---:|
| CPU 频率 | 默认 240MHz | 160MHz |
| 屏幕背光 | 255/255 | 140/255 |
| WiFi 发射功率 | 15dBm | 8.5dBm |
| 已连接控制发送 | 100Hz | 100Hz |
| 未连接搜索发送 | 100Hz | 20Hz |
| LVGL handler 调度 | 主循环每圈 | 5ms |
| 主循环 delay | 1ms | 5ms |
| LVGL 页面数据更新 | 100ms | 200ms |

### 验证结果

| 项目 | 结果 |
|---|---|
| `pio run -e s3_transmitter` | SUCCESS |
| `pio run -e s3_transmitter -t upload --upload-port COM7` | SUCCESS |
| `pio run -e receiver` | SUCCESS |
| `pio run -e transmitter` | SUCCESS |
| `pio run -e s3_lvgl_probe` | SUCCESS |
| `pio test -e native` | PASS，12/12 |
| COM7 串口读取 | SUCCESS |

### 串口关键信息

```text
ESP32-S3 formal transmitter
S3 power profile: CPU 160MHz, LCD brightness 140, WiFi TX 8.5dBm
S3 joystick center: 59
S3 AUX I2C scan
  found 0x0D
  found 0x62
  found 0x76
S3 transmitter MAC: 48:CA:43:9A:A9:B0
S3 THR:0 SPD:1 BTN:00 [LOST] BAT:OK
```

### 待人工确认

- 通电 3 到 5 分钟后，确认芯片是否仍然烫手。
- 目视确认 LVGL 文字撕裂是否改善。
- 如果仍然严重发热，应立即断电，转入硬件供电/短路/背光电流排查。

## LVGL RGB565 字节序复测

### 用户照片判断

- 照片中白色和黄色文字边缘出现红/青彩边。
- 现象不像整屏 tearing，更像 RGB565 字节序或颜色通道传输顺序不匹配。
- 已检查 LVGL 字体：未启用 `LV_USE_FONT_SUBPX`，Montserrat 10/12/14 字体均为 `LV_FONT_SUBPX_NONE`。

### 本次调整

| 参数 | 调整前 | 调整后 |
|---|---:|---:|
| `LV_COLOR_16_SWAP` | 0 | 1 |

### 验证结果

| 项目 | 结果 |
|---|---|
| `pio run -e s3_transmitter -t clean` | SUCCESS |
| `pio run -e s3_transmitter` | SUCCESS，RAM 40.3%，Flash 29.8% |
| `pio run -e s3_transmitter -t upload --upload-port COM7` | SUCCESS |
| COM7 串口读取 | SUCCESS |
| `pio run -e receiver` | SUCCESS |
| `pio run -e transmitter` | SUCCESS |
| `pio run -e s3_lvgl_probe` | SUCCESS |
| `pio test -e native` | PASS，12/12 |

### 待人工确认

- 目视确认文字红/青彩边是否消失。
- 如果彩边变成颜色明显错误或依然存在，下一步改为在 flush 回调中手动转换 RGB565，再与 `LV_COLOR_16_SWAP` 做 A/B 对比。

## LVGL UI 独立层和触摸显示复测

### 本次变更

- 新增 `src/transmitter_s3/ui/ui.h` 和 `src/transmitter_s3/ui/ui.cpp`。
- `main.cpp` 不再持有 dashboard label/bar 对象。
- `main.cpp` 通过 `S3UiState` 向 `ui_update()` 传递页面数据。
- 触摸回调调用 `ui_set_touch()`：
  - pressed：显示坐标和触摸点。
  - released：显示 `TOUCH --` 并隐藏触摸点。

### 验证结果

| 项目 | 结果 |
|---|---|
| `pio run -e s3_transmitter` | SUCCESS，RAM 40.3%，Flash 29.8% |
| `pio run -e s3_transmitter -t upload --upload-port COM7` | SUCCESS |
| COM7 串口读取 | SUCCESS |
| `pio run -e receiver` | SUCCESS |
| `pio run -e transmitter` | SUCCESS |
| `pio run -e s3_lvgl_probe` | SUCCESS |
| `pio test -e native` | PASS，12/12 |

### 串口关键信息

```text
ESP32-S3 formal transmitter
S3 power profile: CPU 160MHz, LCD brightness 140, WiFi TX 8.5dBm
S3 joystick center: 59
S3 AUX I2C scan
  found 0x0D
  found 0x62
  found 0x76
S3 transmitter MAC: 48:CA:43:9A:A9:B0
S3 THR:0 SPD:1 BTN:00 [LOST] BAT:OK
```

### 待人工确认

- 触摸屏幕时，页面应显示 `TOUCH x,y`。
- 洋红色触摸点应跟随手指移动。
- 松手后触摸点应隐藏，文字回到 `TOUCH --`。

## S3 触摸跟手一级优化复测

### 本次变更

| 参数 | 调整前 | 调整后 |
|---|---:|---:|
| LVGL 显示刷新周期 | LVGL 默认 | 16ms |
| LVGL 触摸读取周期 | LVGL 默认 | 5ms |
| LVGL handler 调度 | 5ms | 2ms |
| 主循环空闲延时 | 5ms | 2ms |
| 页面业务数据更新 | 200ms | 200ms |

### 验证结果

| 项目 | 结果 |
|---|---|
| `pio test -e native` | PASS，13/13 |
| `pio run -e s3_transmitter` | SUCCESS，RAM 40.3%，Flash 29.8% |
| `pio run -e s3_transmitter -t upload --upload-port COM7` | SUCCESS |
| COM7 串口读取 | SUCCESS |
| `pio run -e receiver` | SUCCESS |
| `pio run -e transmitter` | SUCCESS |
| `pio run -e s3_lvgl_probe` | SUCCESS |

### 串口关键日志

```text
ESP32-S3 formal transmitter
S3 power profile: CPU 160MHz, LCD brightness 140, WiFi TX 8.5dBm
S3 joystick center: 58
S3 AUX I2C scan
  found 0x0D
  found 0x62
  found 0x76
S3 transmitter MAC: 48:CA:43:9A:A9:B0
S3 THR:0 SPD:1 BTN:00 [LOST] BAT:OK
S3 THR:0 SPD:1 BTN:00 [LOST] BAT:N/A
S3 THR:0 SPD:1 BTN:00 [LOST] BAT:OK
```

### 观察项

- 本次复测期间 CW2015 曾短暂读到 `BAT:N/A`，随后恢复 `BAT:OK`；暂记为 I2C/电量计读取稳定性观察项。
- 仍需人工确认触摸圆点跟手延迟是否降低。
- 仍需人工确认长时间运行温度是否可接受。

## S3 LVGL 显示 DMA 复测

### 本次变更

| 项目 | 调整前 | 调整后 |
|---|---|---|
| LVGL flush 像素推送 | `display.pushImage()` | `display.pushImageDMA()` |
| LVGL buffer 释放时机 | 同步推送后 ready | `display.waitDMA()` 后 ready |
| DMA 配置 | 无显式开关 | `S3_LVGL_DISPLAY_USE_DMA=1` |

### 验证结果

| 项目 | 结果 |
|---|---|
| TDD 红灯 | `pio test -e native` 因缺少 `S3_LVGL_DISPLAY_USE_DMA` 失败 |
| `pio test -e native` | PASS，14/14 |
| `pio run -e s3_transmitter` | SUCCESS，RAM 40.3%，Flash 29.8% |
| `pio run -e s3_transmitter -t upload --upload-port COM7` | SUCCESS |
| COM7 串口读取 | SUCCESS |
| `pio run -e receiver` | SUCCESS |
| `pio run -e transmitter` | SUCCESS |
| `pio run -e s3_lvgl_probe` | SUCCESS |

### 串口关键日志

```text
ESP32-S3 formal transmitter
S3 power profile: CPU 160MHz, LCD brightness 140, WiFi TX 8.5dBm
S3 joystick center: 58
S3 AUX I2C scan
  found 0x0D
  found 0x62
  found 0x76
S3 transmitter MAC: 48:CA:43:9A:A9:B0
S3 THR:0 SPD:1 BTN:00 [LOST] BAT:OK
```

### 待人工确认

- 目视确认 LVGL 页面是否仍然正常，无花屏、残影或闪烁。
- 触摸圆点是否比一级优化后更顺滑。
- 通电 3 到 5 分钟后确认芯片温度是否下降或保持可接受。

## S3 页面 FPS 显示复测

### 本次变更

| 项目 | 结果 |
|---|---|
| FPS 计算 | 按 1000ms 窗口内完成的 LVGL flush 帧数折算 |
| 页面显示 | 底部触摸坐标右侧显示 `FPS n` |
| UI 数据结构 | `S3UiState.displayFps` |

### 验证结果

| 项目 | 结果 |
|---|---|
| TDD 红灯 | `pio test -e native` 因缺少 `displayFpsForFrameCount()` 失败 |
| `pio test -e native` | PASS，15/15 |
| `pio run -e s3_transmitter` | SUCCESS，RAM 40.3%，Flash 29.8% |
| `pio run -e s3_transmitter -t upload --upload-port COM7` | SUCCESS |
| COM7 串口读取 | SUCCESS |
| `pio run -e receiver` | SUCCESS |
| `pio run -e transmitter` | SUCCESS |
| `pio run -e s3_lvgl_probe` | SUCCESS |

### 串口关键日志

```text
ESP32-S3 formal transmitter
S3 power profile: CPU 160MHz, LCD brightness 140, WiFi TX 8.5dBm
S3 joystick center: 58
S3 AUX I2C scan
  found 0x0D
  found 0x62
  found 0x76
S3 transmitter MAC: 48:CA:43:9A:A9:B0
S3 THR:0 SPD:1 BTN:00 [LOST] BAT:OK
```

### 待人工确认

- 底部右侧是否显示 `FPS n`。
- `FPS n` 是否与 `TOUCH x,y` 或底部占位提示重叠。

## S3 发射端与接收端联调复测

### DMA 确认

| 项目 | 结果 |
|---|---|
| `S3_LVGL_DISPLAY_USE_DMA` | `1` |
| LVGL flush | `display.pushImageDMA()` |
| LVGL buffer ready | `display.waitDMA()` 后调用 `lv_disp_flush_ready()` |

### 初次联调现象

| 端 | 日志 | 判断 |
|---|---|---|
| S3 发射端 COM7 | `S3 THR:0 SPD:1 BTN:00 [LOST] BAT:OK` | 未收到状态包 |
| 接收端 COM10 | `THR:   0 SPD:1 BTN:00 [FAILSAFE]` | 未进入稳定连接 |

### 修复内容

- 接收端把 `StatusPacket` 回发目标从固定 C3 发射端 MAC 改为最后一个合法发射器 MAC。
- C3 发射端、S3 发射端、接收端 ESP-NOW peer 信道均显式固定为 1。

### 验证结果

| 项目 | 结果 |
|---|---|
| `pio test -e native` | PASS，16/16 |
| `pio run -e receiver` | SUCCESS |
| `pio run -e transmitter` | SUCCESS |
| `pio run -e s3_transmitter` | SUCCESS，RAM 40.3%，Flash 29.9% |
| `pio run -e s3_lvgl_probe` | SUCCESS |
| `pio run -e receiver -t upload --upload-port COM10` | SUCCESS |
| `pio run -e s3_transmitter -t upload --upload-port COM7` | SUCCESS |
| C3 基础发射端上传 | 未执行，当前无 COM3 |

### 联调日志

S3 发射端 20 秒稳定输出：

```text
S3 THR:0 SPD:1 BTN:00 [OK] BAT:OK
S3 THR:0 SPD:1 BTN:00 [OK] BAT:OK
S3 THR:0 SPD:1 BTN:00 [OK] BAT:OK
```

接收端 20 秒稳定输出：

```text
THR:   0 SPD:1 BTN:00
PWM value: 76 (throttle=0)
```

### 观察项

- S3 曾出现一次 `BROWNOUT_RST`，建议继续观察供电和 USB 线/电源能力。
- 当前 S3 摇杆、按钮、档位 GPIO 仍为软件占位，实体输入功能需等硬件定稿后再完整验证。
