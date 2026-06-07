# Development And Release Workflow

本文件记录本项目的固定推进规则。后续每次代码更新都按这个顺序执行。

## 1. 每次代码更新必须完成

| 顺序 | 动作 | 说明 |
|---|---|---|
| 1 | 修改代码 | 保持改动范围明确，避免混入无关重构 |
| 2 | 运行自动测试 | 至少运行 `pio test -e native` |
| 3 | 编译固件 | 运行 `pio run -e transmitter` 和 `pio run -e receiver` |
| 4 | 上传固件 | 发射端上传到 COM3，接收端上传到 COM10 |
| 5 | 硬件验证 | 按 `docs/test-plan.md` 做相关功能复测 |
| 6 | 完善文档 | 更新规格、参数、bug、测试记录或推进记录 |
| 7 | Git 提交 | 提交代码和文档 |
| 8 | 推送 GitHub | 将本地提交同步推送到远程仓库 |

## 2. 常用命令

### 自动测试

```text
pio test -e native
```

### 编译

```text
pio run -e transmitter
pio run -e receiver
```

### 上传

```text
pio run -e transmitter -t upload --upload-port COM3
pio run -e receiver -t upload --upload-port COM10
```

### Git

```text
git status -sb
git add <files>
git commit -m "<message>"
git push
```

## 3. 推送前检查清单

- [ ] `pio test -e native` 通过。
- [ ] `pio run -e transmitter` 通过。
- [ ] `pio run -e receiver` 通过。
- [ ] 发射端新固件已上传到 COM3。
- [ ] 接收端新固件已上传到 COM10。
- [ ] 已完成本次改动相关的硬件验证。
- [ ] `docs/progress.md` 已记录本次推进。
- [ ] `docs/bugs.md` 已记录新发现或已修复问题。
- [ ] 测试结果已写入 `docs/test-runs/`。
- [ ] 本地提交已推送到 GitHub。

## 4. 当前阻塞

本项目目录当前尚未初始化 git 仓库，且本机尚未安装 GitHub CLI `gh`。首次上传 GitHub 前需要完成：

1. 安装 GitHub CLI。
2. 执行 `gh auth login` 登录。
3. 确认要创建或推送到的 GitHub 仓库，例如 `owner/Fan_Controller_PIO`。
