# ESP-NOW Protocol

## 1. 通信设置

| 项目 | 值 |
|---|---|
| 模式 | WiFi STA |
| WiFi sleep | 关闭 |
| 信道 | 1 |
| 加密 | 否 |
| 配对方式 | 固定 MAC |

## 2. MAC 绑定

当前真实 MAC 以代码为准。

| 方向 | 当前代码值 |
|---|---|
| 发射端发送目标接收端 MAC | `AC:EB:E6:44:C5:90` |
| 接收端绑定发射端 MAC | `AC:EB:E6:44:D5:54` |

## 3. 控制包 `ControlPacket`

发射端到接收端，100Hz。

| 字段 | 类型 | 值/范围 | 说明 |
|---|---|---|---|
| `head` | `uint8_t` | `0xA5` | 帧头 |
| `type` | `uint8_t` | `0x01` | 控制包 |
| `throttle` | `int16_t` | `-1000 ~ 1000` | 油门/刹车 |
| `speedLevel` | `uint8_t` | `1/2/3` | 速度档位 |
| `buttons` | `uint8_t` | bit0/bit1 | 按钮状态 |
| `checksum` | `uint8_t` | 累加和 | 前面所有字节累加低 8 位 |

实际长度：8 字节。

## 4. 状态包 `StatusPacket`

接收端到发射端，20Hz。

| 字段 | 类型 | 值/范围 | 说明 |
|---|---|---|---|
| `head` | `uint8_t` | `0x5A` | 帧头 |
| `type` | `uint8_t` | `0x02` | 状态包 |
| `rssi` | `int16_t` | dBm 或近似值 | 信号强度 |
| `voltage` | `uint16_t` | 电压 x100 | 来自 VESC 输入电压 |
| `motorPWM[4]` | `uint8_t[4]` | 冗余 | 当前无业务用途 |
| `speed` | `uint16_t` | 速度值 | `rpm * 0.00207` |
| `status` | `uint8_t` | bit0 | bit0=失控保护 |
| `checksum` | `uint8_t` | 累加和 | 前面所有字节累加低 8 位 |

实际长度：14 字节。当前代码注释中的 10 字节不准确。

## 5. 校验规则

```text
checksum = sum(packet[0] ... packet[n - 2]) & 0xFF
```

接收方必须检查：

1. 长度是否等于结构体长度。
2. 帧头是否正确。
3. 类型是否正确。
4. 校验和是否正确。
5. 接收端还必须检查发送方 MAC 是否等于绑定发射端 MAC。

## 6. 兼容性记录

`StatusPacket.motorPWM[4]` 已确认为冗余字段。后续如果删除该字段，需要同时修改发射端和接收端，并更新本文件中的包长度。
