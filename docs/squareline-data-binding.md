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
| 连接状态 | Label | `ui_LabelStatus` | `connected` | `lv_label_set_text()` + `lv_obj_set_style_text_color()` |
| VESC 电压文字 | Label | `ui_LabelStatusVoltage` | `connected/receiverVoltageX100` | `lv_label_set_text()` |
| VESC 电压进度 | Arc | `ui_ArcStatusVoltage` | `connected/receiverVoltageX100` | `lv_arc_set_value()` |
| 当前档位 | Label | `ui_LabelControl` | `speedLevel` | `lv_label_set_text()` |
| 油门/刹车进度 | Bar | `ui_BarThrottle` | `joystickValue` | `lv_bar_set_value()` + `lv_obj_set_style_bg_color()` |
| 轮速 | Label | `ui_LabelSpeed` | `receiverSpeed` | `lv_label_set_text()` + `lv_obj_set_style_text_color()` |
| BMP280 | Label | `ui_LabelBmp280` | `bmp280Valid/bmp280PressureHpa/bmp280AltitudeM` | `lv_label_set_text()` |
| QMC5883L 航向 | Label | `ui_LabelHeading` | `qmcValid/qmcHeadingDeg` | `lv_label_set_text()` |
| 按钮/RSSI | Label | `ui_LabelButtons` | `buttonState/rssiValue` | `lv_label_set_text()` |
| FPS | Label | `ui_LabelFps` | `displayFps` | 待添加 SquareLine 控件后绑定 |
| 触摸坐标 | Label | `ui_LabelTouch` | touch callback | 待添加 SquareLine 控件后绑定 |
| 触摸点 | Object | `ui_DotTouch` | touch callback | 待添加 SquareLine 控件后绑定 |

当前 SquareLine 导出的电池 Arc 实例名是 `ui_ArcBattery`，电量文字是 `ui_LabelBattery`，代码已按这两个实际名称绑定。最终以 `src/transmitter_s3/ui/generated/ui_Screen1.h` 中的 `extern lv_obj_t * ...` 声明为准。

当前 SquareLine 导出的速度 Label 实例名是 `ui_LabelSpeed`。速度颜色规则在 `include/s3_ui_bindings.h` 的 `s3SpeedColorHex()` 中维护：低于 15 km/h 为绿色，15-29 km/h 为黄色，30 km/h 及以上为红色。

当前 SquareLine 导出的状态 Label 实例名是 `ui_LabelStatus`。连接时显示 `OK` 并使用绿色；断联时显示 `LOST` 并使用红色。发射端未解锁时不会覆盖连接状态，而是在状态后追加 `LOCK`，例如 `OK LOCK` 或 `LOST LOCK`。VESC 电压由独立的 `ui_LabelStatusVoltage` 显示。

当前 SquareLine 导出的档位 Label 实例名是 `ui_LabelControl`，显示格式为单独档位数字，例如 `1`、`2`、`3`。

当前 SquareLine 导出的油门/刹车 Bar 实例名是 `ui_BarThrottle`。代码侧会强制设置范围为 `-1000` 到 `1000`，模式为 `LV_BAR_MODE_SYMMETRICAL`，因此摇杆回中时 Bar 保持在中位；油门为正值向右增长并使用绿色，刹车为负值向左增长并使用橙色，回中时使用灰色。

当前 SquareLine 导出的 VESC 电压控件是 `ui_ArcStatusVoltage` 和 `ui_LabelStatusVoltage`。Arc 范围固定为 `6-48V`，代码会把 `receiverVoltageX100` 四舍五入到整数伏并限制在该范围内；断联时 Arc 回到 `6V`，文字显示 `--.-V`。

当前 SquareLine 导出的 BMP280 Label 实例名是 `ui_LabelBmp280`，显示格式为两行：第一行 `气压hPa`，第二行 `海拔m`。代码侧会强制设置白色、12px 字体和居中对齐，避免旧手写参数层删除后出现文字看不清或过长挤压。BMP280 的 `0`、`NaN`、越界气压/海拔会被判为不可信，短暂读取失败时继续显示最后一次可信值。海拔由标准海平面气压 `1013.25hPa` 估算，只适合显示趋势，不等同于精密校准海拔。

旧版手写叠加参数层已经删除，包括标题、连接状态、档位/油门、速度、电池、BMP280、航向、按钮/RSSI、FPS、触摸坐标和占位 GPIO 提示。后续新增显示内容应优先在 SquareLine 创建控件，再在 `src/transmitter_s3/ui/ui.cpp` 里绑定数据。

当前 S3 摇杆设置页是 `src/transmitter_s3/ui/ui.cpp` 创建的临时 LVGL 覆盖层，用于按钮 1 进入设置、触摸校准和中位微调。它不写入 `src/transmitter_s3/ui/generated/`，不会被 SquareLine 导出覆盖。后续如果要统一视觉风格，可以在 SquareLine 中创建正式设置页面，再把触摸动作绑定迁移过去。

## 电池 Arc 设置

在 SquareLine 里：

- 添加 Arc。
- Object name 推荐：`ui_ArcBattery`。
- Min value：`0`。
- Max value：`100`。
- 初始 Value：任意，例如 `50`。
- 去掉可点击/可拖动交互，避免触摸误改显示值。
- Main 部分做背景轨道，Indicator 部分做电量颜色。

代码侧会把 `cw2015Soc` 四舍五入并限制到 `0-100`。S3 本地传感器读取层会过滤 `0`、`NaN`、越界等不可信读数，并在短暂 I2C 读取失败时保留最后一次可信值，避免电量 Arc 偶发跳到 `0` 后又恢复。

## 重新导出后的检查

- `src/transmitter_s3/ui/generated/ui.h` 应包含对应对象的声明。
- `src/transmitter_s3/ui/generated/ui_Screen1.c` 应创建对应对象。
- 如果对象名称改变，需要同步修改 `src/transmitter_s3/ui/ui.cpp` 的绑定代码。
- 若编译报 `was not declared in this scope`，优先检查 `ui.cpp` 引用的控件名是否和 `ui_Screen1.h` 中的实际导出名一致。
- 导出后先运行 `pio run -e s3_transmitter`。构建脚本会自动修复 SquareLine 颜色检查和 true color 图片字节序。
