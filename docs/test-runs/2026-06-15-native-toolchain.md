# 2026-06-15 Windows native 工具链修复验证

## 目标

修复当前 Windows 环境下 `pio test -e native` 找不到 `gcc/g++` 的问题，让 Unity native 单测可以直接运行。

## 结果

- 已安装 PlatformIO 工具包 `platformio/toolchain-gccmingw32@1.50100.0`。
- `where gcc` 在全局 PATH 中仍找不到 gcc，说明没有污染系统 PATH。
- 项目通过 `tools/platformio/native_mingw_toolchain.py` 在 `env:native` 构建时注入工具链。
- 清理 `.pio/build/native` 后运行 `pio test -e native`：通过，31/31 PASS。

## 备注

- `env:native` 使用 `-std=gnu++11`，用于兼容项目中的 `nullptr`。
- native 测试程序使用静态链接，避免运行阶段依赖 MinGW runtime DLL 所在路径。
