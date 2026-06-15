# 固件升级流程

本文档记录当前项目的稳定升级流程。换电脑后先安装 VS Code PlatformIO 插件，或在终端确认 `pio --version` 可用。

## 升级前检查

1. 执行 `git pull`，确认本地代码是最新版本。
2. 执行 `pio test -e native`，确保公共控制逻辑单元测试通过。
3. 执行三套固件构建：
   - `pio run -e transmitter`
   - `pio run -e receiver`
   - `pio run -e s3_transmitter`
4. 确认串口：
   - C3 接收端：当前按项目约定使用 `COM4`。
   - S3 高级发射端：按实际设备管理器显示的串口执行，历史常见为 `COM3`。

## 接收端升级

```powershell
pio run -e receiver -t upload --upload-port COM4
```

上传后建议打开串口监视器：

```powershell
pio device monitor --port COM4 --baud 115200
```

接收端启动后应能看到启动日志、MAC 信息、VESC 状态或链路状态。若发射端未开机，接收端应保持安全输出并周期性提示链路异常。

## C3 基础发射端升级

```powershell
pio run -e transmitter -t upload --upload-port <实际串口>
```

上传后确认 OLED 启动、摇杆居中或刹车保持解锁流程正常。

## S3 高级发射端升级

```powershell
pio run -e s3_transmitter -t upload --upload-port <实际串口>
```

上传后确认：

- 主界面状态可刷新。
- Compass 随 QMC5883L 航向旋转。
- BMP280 显示气压/海拔。
- MCU 温度显示类似 `43.2C`。
- 摇杆校准值在重启后仍保留。

## 回滚方式

1. 使用 `git log --oneline` 找到上一个稳定提交。
2. 使用 `git checkout <commit>` 临时切换到稳定版本。
3. 重新执行构建和上传命令。
4. 验证恢复后，再切回 `main`：

```powershell
git checkout main
```

不要使用 `git reset --hard` 回滚工作区，除非明确确认本地没有需要保留的改动。
