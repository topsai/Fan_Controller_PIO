# Unit Test Plan

当前已开始把纯逻辑抽离到 `include/control_logic.h`，并使用 PlatformIO native + Unity 运行单元测试。

## 1. 推荐测试范围

| 模块 | 可测试逻辑 | 说明 |
|---|---|---|
| 校验和 | 数据包 checksum | 不依赖硬件 |
| 摇杆映射 | ADC raw -> throttle | 可覆盖死区、边界、校准中心 |
| 档位判断 | 三个开关输入 -> speedLevel | 可覆盖优先级 3>2>1 |
| 按钮状态 | 按键输入 -> bit mask | 可覆盖按下/松开 |
| PWM 映射 | throttle + speedLevel -> PWM duty | 可覆盖限幅和中位 |
| 状态包解析 | bytes -> status fields | 可覆盖非法长度、非法帧头、校验失败 |
| 失控保护 | lastRecvTime -> failsafe | 可覆盖超时边界 |
| 速度换算 | rpm -> speed | 使用实测系数 0.00207 |

## 2. 当前测试目录结构

```text
include/
  control_logic.h
test/
  test_control_logic/
    test_main.cpp
```

运行命令：

```text
pio test -e native
```

## 3. 用例清单

### 3.1 校验和

| 用例 | 输入 | 期望 |
|---|---|---|
| CHK-01 | 合法控制包前 7 字节 | 返回累加低 8 位 |
| CHK-02 | 修改任意字段后使用旧 checksum | 校验失败 |
| CHK-03 | 合法状态包前 13 字节 | 返回累加低 8 位 |

### 3.2 摇杆映射

| 用例 | 输入 | 期望 |
|---|---|---|
| JOY-01 | raw=center | 0 |
| JOY-02 | raw=center+49 | 0 |
| JOY-03 | raw=center-49 | 0 |
| JOY-04 | raw=4095 | 接近 1000 |
| JOY-05 | raw=0 | 接近 -1000 |
| JOY-06 | 校准中心为实测值 | 使用校准中心而不是固定 2048 |

### 3.3 档位判断

| 用例 | sw1 | sw2 | sw3 | 期望 |
|---|---|---|---|---|
| SPD-01 | true | false | false | 1 |
| SPD-02 | false | true | false | 2 |
| SPD-03 | false | false | true | 3 |
| SPD-04 | true | true | false | 2 |
| SPD-05 | true | true | true | 3 |
| SPD-06 | false | false | false | 1 |

### 3.4 PWM 映射

| 用例 | throttle | speedLevel | 期望 |
|---|---:|---:|---|
| PWM-01 | 0 | 1 | 中位，约 76 |
| PWM-02 | 1000 | 1 | 按 500 限幅 |
| PWM-03 | 1000 | 2 | 按 750 限幅 |
| PWM-04 | 1000 | 3 | 最大值 102 |
| PWM-05 | -1000 | 3 | 最小值 51 |

### 3.5 失控保护

| 用例 | 条件 | 期望 |
|---|---|---|
| FS-01 | 距上次收包 1999ms | 不进入 failsafe |
| FS-02 | 距上次收包 2001ms | 进入 failsafe，throttle=0 |
| FS-03 | failsafe 后收到合法包 | 退出 failsafe |

## 4. 测试工具建议

优先使用 PlatformIO Unity 测试。涉及硬件 API 的部分通过纯函数抽离避免 mock 复杂化。

建议命令：

```text
pio test -e native
```

如果暂时不增加 native 环境，可以先用 `pio test -e transmitter` 和 `pio test -e receiver` 做板载测试。
