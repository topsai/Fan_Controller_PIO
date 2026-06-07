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
