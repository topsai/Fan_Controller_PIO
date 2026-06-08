# Hardware Reference

## 1. 发射端硬件

| 模块 | 说明 | 用途 |
|---|---|---|
| ESP32-C3-DevKitM-1 | 主控 | 发射端控制 |
| 三档拨动开关 | 三路低电平有效输入 | 速度档位 |
| 回中摇杆 | ADC 模拟输入 | 油门/刹车 |
| OLED | SSD1306，I2C，0x3C | 状态显示 |
| CW2015 | I2C，0x62 | 本地电池电压和 SOC |
| 蜂鸣器 | `tone()` 驱动 | 提示音和报警 |
| 按钮 1 | 低电平有效 | 协议保留，暂无业务功能 |
| 按钮 2 | 低电平有效 | 远程控制接收端蜂鸣器 |

## 2. 发射端引脚

| GPIO | 代码名称 | 方向 | 说明 |
|---|---|---|---|
| GPIO6 | `SWITCH_PIN_1` | 输入上拉 | 档位 1，低电平有效 |
| GPIO5 | `SWITCH_PIN_2` | 输入上拉 | 档位 2，低电平有效 |
| GPIO4 | `SWITCH_PIN_3` | 输入上拉 | 档位 3，低电平有效 |
| GPIO8 | `OLED_SDA` | I2C SDA | OLED 和 CW2015 |
| GPIO9 | `OLED_SCL` | I2C SCL | OLED 和 CW2015 |
| GPIO0 | `JOYSTICK_PIN` | ADC 输入 | 回中摇杆 |
| GPIO3 | `BUZZER_PIN` | 输出 | 蜂鸣器 |
| GPIO7 | `BUTTON_1_PIN` | 输入上拉 | 按钮 1 |
| GPIO10 | `BUTTON_2_PIN` | 输入上拉 | 按钮 2 |

## 3. 接收端硬件

| 模块 | 说明 | 用途 |
|---|---|---|
| ESP32-C3-DevKitM-1 | 主控 | 接收端控制 |
| VESC | UART 115200 | 电机控制器和遥测来源 |
| PWM/Servo 输出 | 50Hz | 控制 VESC 油门 |
| 状态 LED | 普通 GPIO 输出 | 连接和失控指示 |
| 蜂鸣器 | `tone()` 驱动 | 失控提示和远程鸣叫 |

## 4. 接收端引脚

| GPIO | 代码名称 | 方向 | 说明 |
|---|---|---|---|
| GPIO4 | `PWM_OUT_1` | PWM 输出 | VESC Servo/PWM 控制 |
| GPIO2 | `LED_STATUS` | 输出 | 状态 LED |
| GPIO3 | `BUZZER_PIN` | 输出 | 蜂鸣器 |
| GPIO6 | `Serial1` TX | UART 输出 | ESP32-C3 TX 到 VESC RX |
| GPIO7 | `Serial1` RX | UART 输入 | ESP32-C3 RX 到 VESC TX |

## 5. 接线原则

1. 两端 ESP32-C3 均需稳定供电。
2. 接收端 ESP32-C3 与 VESC 必须共地。
3. VESC UART 接线按 TX/RX 交叉连接。
4. I2C 总线上的 OLED 和 CW2015 地址分别为 `0x3C`、`0x62`。
5. 当前引脚已经在原 Arduino 工程中验证可运行，迁移时不调整。

## 6. ESP32-S3R8 LVGL 高级版探针硬件

当前 `s3_lvgl_probe` 只用于验证 Arduino + LovyanGFX 是否能直接点亮屏幕并读取触摸，尚未接入遥控器业务逻辑。

| 模块 | 说明 | 用途 |
|---|---|---|
| ESP32-S3R8 | 主控 | 高级版遥控器候选硬件 |
| GC9A01 | 240x240，8080 8bit 并口 | 彩色 UI 显示 |
| CST816 | I2C 触摸 | 触摸输入 |
| 背光 | PWM | 屏幕亮度 |

## 7. ESP32-S3R8 屏幕/触摸探针引脚

| GPIO | 代码名称 | 方向 | 说明 |
|---|---|---|---|
| GPIO6 | `LCD_D0` | 输出 | LCD 8080 数据 0 |
| GPIO12 | `LCD_D1` | 输出 | LCD 8080 数据 1 |
| GPIO5 | `LCD_D2` | 输出 | LCD 8080 数据 2 |
| GPIO11 | `LCD_D3` | 输出 | LCD 8080 数据 3 |
| GPIO4 | `LCD_D4` | 输出 | LCD 8080 数据 4 |
| GPIO10 | `LCD_D5` | 输出 | LCD 8080 数据 5 |
| GPIO3 | `LCD_D6` | 输出 | LCD 8080 数据 6 |
| GPIO9 | `LCD_D7` | 输出 | LCD 8080 数据 7 |
| GPIO7 | `LCD_WR` | 输出 | LCD 8080 写时钟 |
| GPIO14 | `LCD_CS` | 输出 | LCD 片选 |
| GPIO13 | `LCD_DC` | 输出 | LCD 命令/数据 |
| GPIO8 | `LCD_RST` | 输入上拉 | 当前按原 ESP-IDF 工程处理，软件复位禁用 |
| GPIO45 | `LCD_BL` | PWM 输出 | LCD 背光 |
| GPIO41 | `LCD_POWER` | 输出 | LCD 电源使能，当前按原 ESP-IDF 工程推断为低有效 |
| GPIO47 | `LCD_TE` | 输入 | 当前探针未使用 |
| GPIO15 | `TP_SDA` | I2C SDA | CST816 触摸 |
| GPIO16 | `TP_SCL` | I2C SCL | CST816 触摸 |
| GPIO17 | `TP_INT` | 输入 | CST816 触摸中断 |

注意：这些屏幕引脚会占用原 C3 遥控器的多个 GPIO，因此高级版遥控器的摇杆、按钮、档位、CW2015 和蜂鸣器必须重新规划 S3 引脚。

## 8. ESP32-S3R8 外设 I2C 总线

用户补充确认：高级版遥控器开发板上另有一组外设 I2C 总线。

| GPIO | 代码名称 | 方向 | 挂载芯片 |
|---|---|---|---|
| GPIO18 | `AUX_I2C_SDA` | I2C SDA | CW2015、BMP280、LSM6DSLTR、QMC5883L |
| GPIO19 | `AUX_I2C_SCL` | I2C SCL | CW2015、BMP280、LSM6DSLTR、QMC5883L |
| GPIO20 | `INT1_LSM` | 输入 | LSM6DSLTR INT1 中断脚 |
| GPIO21 | `INT2_LSM` | 输入 | LSM6DSLTR INT2 中断脚 |

注意：CST816 触摸使用 GPIO15/GPIO16，不与 GPIO18/GPIO19 这组外设 I2C 共用引脚。后续高级版遥控器需要在软件中明确分配 I2C 控制器，避免触摸和传感器初始化互相覆盖。
