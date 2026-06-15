# ESP-NOW 协议 v2

本文档记录 2026-06-15 起当前固件使用的协议格式。旧版 `docs/protocol.md` 中的 checksum/v1 描述仅作为历史记录保留，当前实现以 `include/protocol.h` 和本文档为准。

## 通用规则

- 通信方式：ESP-NOW，WiFi STA，固定 MAC 配对。
- 控制包方向：发射端到接收端，约 100Hz。
- 状态包方向：接收端到发射端，约 20Hz。
- 校验：CRC-8，多项式 `0x07`，计算范围为结构体中除最后 `crc` 字段外的全部字节。
- 版本：控制包和状态包都使用 `version = 2`。
- 序号：`uint16_t sequence`，接收方只接受比上一次更新鲜的序号，用于拒绝重复包、乱序包和简单重放。

## ControlPacket

| 字段 | 类型 | 当前值/范围 | 说明 |
|---|---|---|---|
| `head` | `uint8_t` | `0xA5` | 控制包帧头 |
| `type` | `uint8_t` | `0x01` | 控制包类型 |
| `version` | `uint8_t` | `2` | 协议版本 |
| `sequence` | `uint16_t` | 自增 | 控制包序号 |
| `throttle` | `int16_t` | `-1000..1000` | 油门/刹车 |
| `speedLevel` | `uint8_t` | `1/2/3` | 速度档位 |
| `buttons` | `uint8_t` | bit mask | 远程按钮状态 |
| `flags` | `uint8_t` | bit mask | 发射端状态；未解锁时带 `OUTPUT_LOCKED` |
| `crc` | `uint8_t` | CRC-8 | 包校验 |

## StatusPacket

| 字段 | 类型 | 当前值/范围 | 说明 |
|---|---|---|---|
| `head` | `uint8_t` | `0x5A` | 状态包帧头 |
| `type` | `uint8_t` | `0x02` | 状态包类型 |
| `version` | `uint8_t` | `2` | 协议版本 |
| `sequence` | `uint16_t` | 自增 | 状态包序号 |
| `rssi` | `int16_t` | dBm 或近似值 | 接收端看到的链路信号 |
| `voltage` | `uint16_t` | 电压 x100 | VESC 输入电压 |
| `motorPWM[4]` | `uint8_t[4]` | 暂保留 | 历史字段，当前业务不依赖 |
| `speed` | `uint16_t` | 当前速度值 | 来自 VESC 遥测换算 |
| `status` | `uint8_t` | bit mask | 接收端安全/诊断状态 |
| `crc` | `uint8_t` | CRC-8 | 包校验 |

## 状态位

| 标志 | 值 | 含义 |
|---|---:|---|
| `STATUS_FLAG_FAILSAFE` | `0x01` | 接收端进入失控保护 |
| `STATUS_FLAG_VESC_VALID` | `0x02` | VESC 遥测当前有效 |
| `STATUS_FLAG_PROTOCOL_FAULT` | `0x04` | 接收端最近检测到协议错误 |
| `STATUS_FLAG_OUTPUT_LOCKED` | `0x08` | 输出处于锁定或源端未解锁 |

## 接收端安全门限

接收端收到合法控制包后不会立即认为链路稳定。当前要求连续收到 3 个合法、版本正确、CRC 正确、序号更新鲜的控制包，才会进入 connected 并应用油门输出。任何协议错误、失控或断联都会清零稳定计数并将输出锁定。

## v1 迁移兼容

接收端保留旧 v1 控制包兼容路径。旧遥控器发送的 7 字节控制包仍按累加和校验接收，并回传旧 14 字节状态包。新遥控器发送 v2 控制包时，接收端继续使用 v2 状态包、CRC-8、序号和状态位。

这个兼容层用于避免只升级接收端时遥控器完全断联。长期仍建议发射端和接收端都升级到 v2。

## S3 诊断使用

- `receiverStatusFlags`：接收端状态位。
- `statusPacketRateHz`：状态包接收速率。
- `statusLostPackets`：按序号差估算的状态包丢失数。
- `rssiValue`：接收端回传 RSSI。
