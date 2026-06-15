# 当前项目状态

更新日期：2026-06-15

## 当前应优先阅读的文档

换电脑或新开 Codex 会话后，建议按这个顺序恢复上下文：

1. `docs/current-status.md`
2. `docs/productization-roadmap.md`
3. `docs/protocol-v2.md`
4. `docs/s3-ui-final-components.md`
5. `docs/regression-matrix.md`
6. `docs/firmware-upgrade.md`
7. `docs/codex-handoff.md`

## 当前固件能力

- C3 基础发射端、C3 接收端、S3 高级发射端均可编译。
- ESP-NOW 控制协议已升级到 v2，使用版本号、CRC-8 和序号防重放。
- 接收端兼容旧 v1 遥控器包；只升级接收端时不会再因为 v1/v2 包长不同而完全断联。
- 接收端在连续 3 个合法控制包后才应用输出，协议错误或失控时进入保守安全状态。
- S3 摇杆校准持久化到 NVS，重启后保留。
- S3 主界面已经绑定 Compass、BMP280、MCU 温度和核心状态。
- S3 诊断数据已经准备好，后续 UI 添加控件即可继续绑定。
- 接收端当前约定端口为 COM4。
- 已新增串口诊断注入测试脚本，10 秒默认运行，30 分钟稳定性测试必须手动 `--long` 触发。

## 本轮验证结果

- `pio test -e native`：PASS，39/39。
- `pio run -e transmitter`：SUCCESS。
- `pio run -e receiver`：SUCCESS。
- `pio run -e s3_transmitter`：SUCCESS。
- `pio run -e receiver -t upload --upload-port COM4`：SUCCESS，设备 MAC `AC:EB:E6:44:C5:90`。

## UI 后续添加

最终需要新增哪些页面和控件，见 `docs/s3-ui-final-components.md`。代码层已经预留：

- 接收端状态位。
- RSSI。
- 状态包接收速率。
- 状态包丢包计数。
- 屏幕亮度状态。
- MCU 温度预警。

添加 SquareLine 控件后，只需要继续在 `src/transmitter_s3/ui/ui.cpp` 里按控件名绑定显示即可。

## 连通性测试入口

见 `docs/connectivity-test.md`。常用 10 秒测试：

```powershell
python tools/diagnostics/link_test.py --receiver-port COM4 --remote-port COM3
```
