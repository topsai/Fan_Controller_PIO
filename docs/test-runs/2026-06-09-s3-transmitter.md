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
