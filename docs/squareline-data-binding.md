# SquareLine 数据绑定指南

本项目采用常规 LVGL/SquareLine 分工：

- SquareLine 负责页面、控件、图片、字体、颜色、位置和静态样式。
- 固件代码负责读取硬件、计算状态、把数据写入 SquareLine 生成的控件。
- 不在 `src/transmitter_s3/ui/generated/` 里手写业务逻辑，因为重新导出会覆盖该目录。

## 操作流程

1. 在 SquareLine 里添加控件。
2. 给需要由代码更新的控件设置稳定名称。
3. 设置控件的范围、初始值和静态样式。
4. 导出到 `src/transmitter_s3/ui/generated/`。
5. 运行 `pio run -e s3_transmitter`。
6. 在 `src/transmitter_s3/ui/ui.cpp` 中把 `S3UiState` 数据绑定到生成控件。

SquareLine 不能直接获取 CW2015、VESC、摇杆、按钮等真实硬件数据。它只生成 `lv_obj_t *` 控件变量，真实数据由 `src/transmitter_s3/main.cpp` 读取并填入 `S3UiState`，最后由 `s3_ui_update()` 写入控件。

## 控件命名建议

| 显示内容 | SquareLine 控件类型 | 推荐对象名 | 当前数据来源 | 绑定代码 |
| --- | --- | --- | --- | --- |
| 电池电量百分比 | Arc | `ui_ArcBattery` | `S3UiState::cw2015Soc` | `lv_arc_set_value()` |
| 电池电量文字 | Label | `ui_LabelBattery` | `cw2015Valid/cw2015Voltage/cw2015Soc` | `lv_label_set_text()` |
| 连接状态/VESC 电压 | Label | `ui_LabelStatus` | `connected/receiverVoltageX100` | `lv_label_set_text()` |
| 档位和油门 | Label | `ui_LabelControl` | `speedLevel/joystickValue` | `lv_label_set_text()` |
| 油门/刹车进度 | Bar 或 Arc | `ui_BarThrottle` | `joystickValue` | `lv_bar_set_value()` |
| 轮速 | Label | `ui_LabelSpeed` | `receiverSpeed` | `lv_label_set_text()` + `lv_obj_set_style_text_color()` |
| BMP280 | Label | `ui_LabelBmp280` | `bmp280Valid/bmp280TemperatureC/bmp280PressureHpa` | `lv_label_set_text()` |
| QMC5883L 航向 | Label | `ui_LabelHeading` | `qmcValid/qmcHeadingDeg` | `lv_label_set_text()` |
| 按钮/RSSI | Label | `ui_LabelButtons` | `buttonState/rssiValue` | `lv_label_set_text()` |
| FPS | Label | `ui_LabelFps` | `displayFps` | `lv_label_set_text()` |
| 触摸坐标 | Label | `ui_LabelTouch` | touch callback | `lv_label_set_text()` |
| 触摸点 | Object | `ui_DotTouch` | touch callback | `lv_obj_set_pos()` |

当前 SquareLine 导出的电池 Arc 实例名是 `ui_uiArcBattery`，电量文字是 `ui_uiLabelBattery`，代码已按这两个实际名称绑定。推荐名仍然是 `ui_ArcBattery` / `ui_LabelBattery`，但最终以 `src/transmitter_s3/ui/generated/ui_Screen1.h` 中的 `extern lv_obj_t * ...` 声明为准。

当前 SquareLine 导出的速度 Label 实例名是 `ui_uiLabelSpeed`。速度颜色规则在 `include/s3_ui_bindings.h` 的 `s3SpeedColorHex()` 中维护：低于 15 km/h 为绿色，15-29 km/h 为黄色，30 km/h 及以上为红色。

## 电池 Arc 设置

在 SquareLine 里：

- 添加 Arc。
- Object name 推荐：`ui_ArcBattery`。
- Min value：`0`。
- Max value：`100`。
- 初始 Value：任意，例如 `50`。
- 去掉可点击/可拖动交互，避免触摸误改显示值。
- Main 部分做背景轨道，Indicator 部分做电量颜色。

代码侧会把 `cw2015Soc` 四舍五入并限制到 `0-100`，无效电量显示为 `0`。

## 重新导出后的检查

- `src/transmitter_s3/ui/generated/ui.h` 应包含对应对象的声明。
- `src/transmitter_s3/ui/generated/ui_Screen1.c` 应创建对应对象。
- 如果对象名称改变，需要同步修改 `src/transmitter_s3/ui/ui.cpp` 的绑定代码。
- 若编译报 `was not declared in this scope`，优先检查 `ui.cpp` 引用的控件名是否和 `ui_Screen1.h` 中的实际导出名一致。
- 导出后先运行 `pio run -e s3_transmitter`。构建脚本会自动修复 SquareLine 颜色检查和 true color 图片字节序。
