# S3 UI Workflow

## 目录约定

| 内容 | 路径 | 是否提交 |
|---|---|---|
| SquareLine Studio 原始工程 | `tools/ui_projects/s3_transmitter_squareline/` | 提交，便于换电脑继续编辑 |
| SquareLine 缓存/备份 | `tools/ui_projects/**/cache/`、`tools/ui_projects/**/backup/` | 不提交 |
| SquareLine 导出的 LVGL 代码 | `src/transmitter_s3/ui/generated/` | 提交，参与固件编译 |
| S3 业务 UI 适配层 | `src/transmitter_s3/ui/ui.cpp`、`src/transmitter_s3/ui/ui.h` | 提交，手写维护 |

## 导出流程

1. 在 SquareLine Studio 中编辑 `tools/ui_projects/s3_transmitter_squareline/`。
2. 导出 LVGL 代码到 `src/transmitter_s3/ui/generated/`。
3. 不手动修改 `generated/` 里的颜色检查或函数名。
4. 运行 `pio run -e s3_transmitter` 验证生成代码能编译。

## 集成边界

SquareLine 生成层保留原始函数名：

```c
ui_init();
ui_destroy();
```

项目业务适配层使用 S3 专用函数名：

```cpp
s3_ui_init();
s3_ui_update(state);
s3_ui_set_touch(pressed, x, y);
```

`s3_ui_init()` 会先调用 SquareLine 生成的 `ui_init()`，再创建遥控器状态、触摸点和 FPS 等业务显示元素。这样重新导出 SquareLine 页面时，不会覆盖通信、传感器、状态更新等业务逻辑。

## 自动补丁

当前实机显示需要：

```c
LV_COLOR_16_SWAP = 1
```

SquareLine 1.6.1 导出的 `ui.c` 会默认检查 `LV_COLOR_16_SWAP == 0`。项目已通过 `tools/platformio/patch_squareline_export.py` 在 `s3_transmitter` 编译前自动屏蔽该检查，因此重新导出后不需要手动修 `generated/ui.c`。

如果将来改成其他屏幕驱动或颜色顺序，需要重新验证：

- 字体是否有红/蓝彩边。
- 颜色是否正常。
- `LV_COLOR_16_SWAP` 与 flush 逻辑是否匹配。

## 注意事项

- 不建议手动改 `src/transmitter_s3/ui/generated/` 中的业务逻辑，因为下次导出会覆盖。
- 需要持久保留的页面对象命名，应在 SquareLine 中设置清晰名称，再由适配层引用。
- 生成页面当前可以作为背景或主布局；遥控器实时数据仍由适配层统一更新。
