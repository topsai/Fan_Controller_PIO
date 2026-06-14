# Fan Controller PIO Documentation

本目录记录 Fan Controller 从 Arduino IDE 迁移到 VSCode + PlatformIO 后的项目资料。

## 文档索引

| 文件 | 用途 |
|---|---|
| `requirements.md` | 完整需求规格书，作为当前功能基线 |
| `user-guide.md` | 日常使用方法、功能说明、提示音、测试和上传命令 |
| `hardware.md` | 硬件清单、引脚定义、接线说明 |
| `protocol.md` | ESP-NOW 数据包、字段、校验和、MAC 绑定 |
| `state-machines.md` | 发射端和接收端状态机 |
| `parameters.md` | 控制频率、阈值、PWM、VESC、OLED 等参数表 |
| `test-plan.md` | 编译、烧录、单板测试、联调测试、回归测试步骤 |
| `test-runs/` | 实际测试执行记录 |
| `unit-test-plan.md` | 后续可自动化测试的设计和用例清单 |
| `workflow.md` | 每次代码更新、测试、上传固件、文档和 GitHub 推送流程 |
| `codex-handoff.md` | 换电脑或新 Codex 会话继续开发时的交接入口 |
| `ui-workflow.md` | S3 SquareLine Studio 工程、导出目录、生成代码和适配层边界 |
| `squareline-data-binding.md` | SquareLine 控件命名、数据来源和代码绑定规则 |
| `progress.md` | 项目推进记录 |
| `bugs.md` | bug/风险/维护项跟踪 |
| `decisions.md` | 已确认设计决策和依据 |

## 当前基线

- 原 Arduino 代码已经是可运行状态。
- 当前 PIO 项目保留发射端和接收端两个环境：
  - `transmitter`
  - `receiver`
  - `s3_lvgl_probe`：ESP32-S3R8 触摸屏高级版遥控器的显示/触摸探针环境
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
7. 如果阶段性工作会影响下一次接手，更新 `codex-handoff.md` 中的当前状态或待确认事项。
