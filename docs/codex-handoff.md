# Codex Handoff

本文件用于在不同电脑、不同 VS Code/Codex 会话之间继续开发项目。它不是完整聊天记录，而是长期同步的项目工作记忆入口。

## 换电脑后先做什么

1. 拉取最新代码：

```text
git pull
```

2. 查看当前工作区状态：

```text
git status -sb
```

3. 阅读这些文档，按顺序恢复上下文：

| 顺序 | 文件 | 目的 |
|---|---|---|
| 1 | `docs/README.md` | 文档总入口和维护规则 |
| 2 | `docs/progress.md` | 最近推进记录、已完成工作、下一步 |
| 3 | `docs/bugs.md` | 当前 bug、风险和 Watch 项 |
| 4 | `docs/decisions.md` | 已确认设计决策，避免重复讨论 |
| 5 | `docs/workflow.md` | 测试、上传、提交、推送流程 |
| 6 | `docs/test-runs/` | 实际硬件验证记录 |

4. 如果继续 S3 触摸屏高级遥控器相关工作，还要读：

| 文件 | 目的 |
|---|---|
| `docs/ui-workflow.md` | SquareLine 工程、导出目录、生成代码边界 |
| `docs/squareline-data-binding.md` | UI 控件命名、数据绑定和适配层规则 |
| `docs/s3-ui-final-components.md` | S3 最终页面和组件对象名清单 |
| `docs/superpowers/plans/` | 已有阶段计划和 UI/硬件推进思路 |

## 当前项目状态记录

截至 2026-06-15，本地 `main` 分支跟踪 `origin/main`。换电脑后以 GitHub 上最新 `main` 为准：

```text
git pull
```

当前代码和文档已经覆盖：

- C3 基础发射端。
- C3 接收端。
- ESP32-S3R8 触摸屏高级发射端。
- SquareLine Studio 工程和导出后的 LVGL 代码。
- 原生单元测试、硬件上传记录和实测记录。
- 多遥控器主动接管：非 active 遥控器按钮1长按 3 秒才可抢占，短按仍进入/退出设置页。
- S3 UI 固件侧产品化绑定：主屏、诊断页、校准页、系统页对象名已稳定；缺失组件会自动跳过。
- 当前 COM3 实物继续使用 `s3_transmitter` 环境和旧 S3 引脚。
- 新版本未打板 PCB 使用 `s3_transmitter_new_pcb` 环境，档位输入为 `GPIO2` ADC 电阻分压。

当前已知需要继续人工观察或复测的重点：

- S3 实物 UI 显示效果，包括 BMP280、电量、状态文字、指南针、MCU 温度和颜色是否稳定。
- S3 新硬件打板前复核 `GPIO0`、`GPIO45`、`GPIO46` 上下拉和上电默认电平。
- 档位 ADC 阈值需要在实物上根据 `ui_LabelSpeedAdc` 或串口读数确认。
- S3 间歇性断联修复后的长期运行稳定性。
- `docs/bugs.md` 中状态为 `Watch` 或 `Open` 的条目。

## 本机相关信息

以下信息可能会随电脑、USB 口、驱动或硬件连接方式变化。换电脑后必须重新确认，不要盲目照抄。

| 项目 | 当前常用值 | 说明 |
|---|---|---|
| C3 基础版发射端上传口 | `COM5` | 换电脑后可能变化 |
| C3 接收端上传口 | `COM4` | 换电脑后可能变化 |
| S3 高级发射端上传口 | `COM3` | 换电脑后可能变化 |
| 当前 S3 实物编译环境 | `s3_transmitter` | 不要用新 PCB 环境烧录当前 COM3 旧板 |
| 新版本 S3 PCB 编译环境 | `s3_transmitter_new_pcb` | 仅新 PCB 打板后使用 |
| PlatformIO 构建目录 | `.pio/` | 本地生成，不提交 |
| VS Code 项目配置 | `.vscode/extensions.json`、`.vscode/settings.json` | 可提交项目通用设置，不写本机绝对路径 |
| VS Code 本机生成配置 | `.vscode/c_cpp_properties.json`、`.vscode/launch.json` | PlatformIO 自动生成，含本机路径，不提交 |
| Codex 完整会话 | 用户目录下 `.codex/sessions/` | 不建议提交到项目仓库 |

换电脑后可以先用 PlatformIO 或设备管理器确认串口，再更新本地命令中的 `--upload-port`。

## 给下一次 Codex 的启动提示

如果要让新的 Codex 会话直接接上项目，可以这样开头：

```text
请先阅读 docs/codex-handoff.md、docs/README.md、docs/progress.md、docs/bugs.md 和 docs/workflow.md，然后根据当前 git 状态继续开发。尽量使用中文沟通和更新文档。
```

如果任务和 S3 UI 有关，再补一句：

```text
这次任务涉及 S3 UI，请同时阅读 docs/ui-workflow.md、docs/squareline-data-binding.md 和 docs/s3-ui-final-components.md。
```

## 每次阶段性结束前

为了让下一台电脑能继续接上，每次阶段性工作结束前至少完成：

- 更新 `docs/progress.md`，写清楚目标、修改、验证、下一步。
- 如果有新 bug 或风险，更新 `docs/bugs.md`。
- 如果改变了硬件、协议、参数、状态机或 UI 绑定，更新对应专题文档。
- 如果做过实物测试，补充 `docs/test-runs/`。
- 按 `docs/workflow.md` 运行必要验证。
- 提交并推送到 GitHub。

完整 Codex 聊天记录可以作为临时参考，但长期同步应以项目文档、代码、测试记录和 Git 提交为准。
