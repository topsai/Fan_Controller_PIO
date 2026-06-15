# ESP-NOW 协议 v2

本文档记录 2026-06-15 起当前固件使用的协议格式。旧版 `docs/protocol.md` 中的 checksum/v1 描述仅作为历史记录保留，当前实现以 `include/protocol.h` 和本文档为准。

## 通用规则

- 通信方式：ESP-NOW，WiFi STA，固定 MAC 配对。
- 控制包方向：发射端到接收端，约 100Hz。
- 状态包方向：接收端到发射端，约 20Hz。
- 校验：CRC-8，多项式 `0x07`，计算范围为结构体中除最后 `crc` 字段外的全部字节。
- 版本：控制包和状态包都使用 `version = 2`。
- 序号：`uint16_t sequence`，接收方只接受比上一包更新鲜的序号，用于拒绝重复包、乱序包和简单重放。

## ControlPacket

| 字段 | 类型 | 当前值/范围 | 说明 |
| --- | --- | --- | --- |
| `head` | `uint8_t` | `0xA5` | 控制包帧头 |
| `type` | `uint8_t` | `0x01` | 控制包类型 |
| `version` | `uint8_t` | `2` | 协议版本 |
| `sequence` | `uint16_t` | 自增 | 控制包序号 |
| `throttle` | `int16_t` | `-1000..1000` | 油门/刹车 |
| `speedLevel` | `uint8_t` | `1/2/3` | 速度档位 |
| `buttons` | `uint8_t` | bit mask | 远程按钮状态 |
| `flags` | `uint8_t` | bit mask | 发射端控制标志 |
| `crc` | `uint8_t` | CRC-8 | 包校验 |

### 控制标志位

| 标志 | 值 | 含义 |
| --- | ---: | --- |
| `CONTROL_FLAG_TAKEOVER_REQUEST` | `0x01` | 非 active 遥控器请求主动接管控制权 |

`CONTROL_FLAG_TAKEOVER_REQUEST` 只存在于控制包，不和状态包的 `STATUS_FLAG_OUTPUT_LOCKED` 混用。

## StatusPacket

| 字段 | 类型 | 当前值/范围 | 说明 |
| --- | --- | --- | --- |
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

### 状态标志位

| 标志 | 值 | 含义 |
| --- | ---: | --- |
| `STATUS_FLAG_FAILSAFE` | `0x01` | 接收端进入失控保护 |
| `STATUS_FLAG_VESC_VALID` | `0x02` | VESC 遥测当前有效 |
| `STATUS_FLAG_PROTOCOL_FAULT` | `0x04` | 接收端最近检测到协议错误 |
| `STATUS_FLAG_OUTPUT_LOCKED` | `0x08` | 输出处于锁定或源端未解锁 |

## 接收端安全门限

接收端收到合法控制包后不会立刻认为链路稳定。当前要求连续收到 3 个合法、版本正确、CRC 正确、序号更新鲜的控制包，才会进入 `connected` 并应用油门输出。任何协议错误、失控或断联都会清零稳定计数并将输出锁定。

## 多遥控器主动切换

接收端维护当前控制源：

- `activeControllerMac`：当前 active 遥控器 MAC。
- `activeUsesLegacyProtocol`：当前 active 是否为 v1 旧协议。
- `lastSeenMs`：最近一次合法 active 包时间。

控制源选择规则：

- 没有 active 控制源时，任一绑定且合法的 C3/S3 遥控器可成为 active。
- active 在线时，只接受 active MAC 的普通控制包。
- 非 active 遥控器的普通控制包会被忽略，并计入诊断 `ignored`。
- 非 active 遥控器只有发送 `CONTROL_FLAG_TAKEOVER_REQUEST` 才能主动接管。
- active 超过 `FAILSAFE_TIMEOUT` 未收到合法包后，接收端释放 active 并进入 failsafe。

接管安全流程：

- 收到合法接管请求后，接收端先将输出归零并重置稳定门限。
- 新控制源仍需连续 3 个合法控制包后才恢复输出。
- 接管成功后，状态回包目标切换为新的 active 控制源。
- 序号检查在控制源切换时重置，避免新遥控器因自身序号较低被旧 active 的序号状态误拒绝。

按钮约定：

- 基础版 C3 和 S3 的按钮1短按：进入/退出设置页。
- 按钮1长按 3 秒：发送约 1 秒 `CONTROL_FLAG_TAKEOVER_REQUEST`，不触发设置页。
- S3 触摸设置页入口保持独立，不影响按钮1接管行为。

## v1 迁移兼容

接收端保留旧 v1 控制包兼容路径。旧遥控器发送的 7 字节控制包仍按累加和校验接收，并回传旧 14 字节状态包。

兼容边界：

- v1 遥控器只能在“无 active 控制源”时成为 active。
- v1 遥控器不能主动抢占，因为旧包没有 `flags` 字段。
- 需要主动接管能力时，基础版 C3 和 S3 都必须升级到当前 v2 固件。
