# Fan Controller PIO Documentation

本目录记录 Fan Controller 从 Arduino IDE 迁移到 VSCode + PlatformIO 后的项目资料。

## 文档索引

| 文件 | 用途 |
|---|---|
| `requirements.md` | 完整需求规格书，作为当前功能基线 |
| `hardware.md` | 硬件清单、引脚定义、接线说明 |
| `protocol.md` | ESP-NOW 数据包、字段、校验和、MAC 绑定 |
| `state-machines.md` | 发射端和接收端状态机 |
| `parameters.md` | 控制频率、阈值、PWM、VESC、OLED 等参数表 |
| `test-plan.md` | 编译、烧录、单板测试、联调测试、回归测试步骤 |
| `test-runs/` | 实际测试执行记录 |
| `unit-test-plan.md` | 后续可自动化测试的设计和用例清单 |
| `workflow.md` | 每次代码更新、测试、上传固件、文档和 GitHub 推送流程 |
| `progress.md` | 项目推进记录 |
| `bugs.md` | bug/风险/维护项跟踪 |
| `decisions.md` | 已确认设计决策和依据 |

## 当前基线

- 原 Arduino 代码已经是可运行状态。
- 当前 PIO 项目保留发射端和接收端两个环境：
  - `transmitter`
  - `receiver`
- 开发板：Espressif ESP32-C3-DevKitM-1
- framework：Arduino
- 通信：ESP-NOW 固定 MAC 配对。

## 维护规则

1. 硬件和协议变化先更新 `hardware.md` / `protocol.md`。
2. 行为变化先更新 `requirements.md` 和 `state-machines.md`。
3. 参数变化必须同步更新 `parameters.md`。
4. 修复 bug 时，在 `bugs.md` 添加记录，并在 `test-plan.md` 或 `unit-test-plan.md` 补充验证方法。
5. 每次阶段性推进，在 `progress.md` 增加日期、目标、结果、下一步。
6. 每次代码更新必须按 `workflow.md` 完成测试、文档、固件上传和 GitHub 推送。
