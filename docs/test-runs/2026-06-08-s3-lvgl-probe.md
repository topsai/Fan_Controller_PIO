# 2026-06-08 ESP32-S3R8 Display And Touch Probe

## 背景

目标是把另一块 ESP32-S3R8 触摸屏开发板纳入当前项目，作为后续 LVGL 高级版遥控器的候选硬件。当前阶段只验证 Arduino + PlatformIO 下能否点亮屏幕并读取触摸。

## 方案

优先使用成熟 Arduino 图形库，避免一开始就手写 8080 LCD 和 CST816 触摸驱动。

本轮选择：

| 项目 | 选择 |
|---|---|
| PlatformIO 环境 | `s3_lvgl_probe` |
| 开发板配置 | `esp32-s3-devkitc-1` |
| framework | Arduino |
| 图形库 | `lovyan03/LovyanGFX@1.2.21` |
| 上传端口 | COM7 |

## 屏幕和触摸引脚

| 模块 | 引脚 |
|---|---|
| LCD D0-D7 | GPIO6, GPIO12, GPIO5, GPIO11, GPIO4, GPIO10, GPIO3, GPIO9 |
| LCD WR | GPIO7 |
| LCD CS | GPIO14 |
| LCD DC | GPIO13 |
| LCD RST | GPIO8，当前输入上拉，软件复位禁用 |
| LCD 背光 | GPIO45 |
| LCD 电源 | GPIO41 |
| LCD TE | GPIO47，当前未使用 |
| CST816 SDA/SCL | GPIO15 / GPIO16 |
| CST816 INT | GPIO17 |

## 编译和上传

| 项目 | 结果 |
|---|---|
| `pio run -e s3_lvgl_probe` | SUCCESS |
| `pio run -e s3_lvgl_probe -t upload --upload-port COM7` | SUCCESS |
| `pio run -e transmitter -t upload --upload-port COM3` | SUCCESS |
| `pio run -e receiver -t upload --upload-port COM10` | SUCCESS |

上传识别信息：

```text
Chip is ESP32-S3 (QFN56) (revision v0.2)
Features: WiFi, BLE, Embedded PSRAM 8MB (AP_3v3)
MAC: 48:ca:43:9a:a9:b0
```

## 串口结果

COM7 已抓到应用启动日志：

```text
ESP32-S3 LVGL hardware probe
Display: GC9A01 240x240 8080 8-bit
Touch: CST816 I2C
Display init done. Touch the screen to print coordinates.
```

COM7 已抓到有效触摸坐标：

```text
TOUCH x=1 y=169
TOUCH x=1 y=157
TOUCH x=238 y=101
TOUCH x=238 y=239
TOUCH x=238 y=150
```

## 当前结论

- Arduino + PlatformIO 可以编译并上传到这块 ESP32-S3R8 开发板。
- LovyanGFX 可以作为第一阶段 GC9A01 8080 并口屏幕和 CST816 触摸的候选库。
- 触摸链路已经有有效坐标输出。
- 坐标方向、边缘映射和偶发越界值需要在下一步做校准。

## 待人工确认

1. 屏幕是否显示红/绿/蓝/白测试色块。
2. 屏幕是否显示圆形边框、中心十字和 `S3 DISPLAY` / `TOUCH TEST` 文字。
3. 触摸时屏幕上的紫色点是否跟随手指移动。
4. 如果颜色反相、红蓝颠倒或画面旋转不对，需要调整 `invert`、`rgb_order` 或 `setRotation()`。

## 黑屏排查更新

用户反馈：上传后触摸串口输出存在，但屏幕为黑屏，没有任何显示。

对比原 ESP-IDF 工程后发现：

- 原工程中 GPIO41 `LCD_POWER` 只配置为输出，没有主动拉高。
- 当前 Arduino 探针之前把 GPIO41 拉高。
- 因此初步判断 GPIO41 的 LCD 电源使能为低有效。

本次修改：

- `LCD_POWER` 改为低有效输出。
- 重新上传 `s3_lvgl_probe` 到 COM7。
- COM7 应用启动日志正常。

验证结果：

| 项目 | 结果 |
|---|---|
| `pio run -e s3_lvgl_probe` | SUCCESS |
| `pio run -e s3_lvgl_probe -t upload --upload-port COM7` | SUCCESS |
| COM7 启动日志 | 正常 |
| `pio test -e native` | PASS，12/12 |
| `pio run -e transmitter` | SUCCESS |
| `pio run -e receiver` | SUCCESS |
| `pio run -e transmitter -t upload --upload-port COM3` | SUCCESS |
| `pio run -e receiver -t upload --upload-port COM10` | SUCCESS |

仍需人工目视确认：GPIO41 低有效修改后屏幕是否已经点亮。
