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
