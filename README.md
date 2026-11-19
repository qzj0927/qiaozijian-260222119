# ec-round2 — CVIR 机甲大师 2026 电控组二轮考核

四自由度舵机机械臂（底座 / 肩 / 肘 / 夹爪）的 STM32 固件。
技术主线：**STM32F401CCU6 + 4×PWM + I²C(MPU6050) + USB CDC/Custom HID + CMake**。

> 任务书要求：必做 T0~T5a；选做 T5b / T6 / A1 / A2。
> 本仓库实现了 T0~T5a、T6，以及 T5b 的完整传输层（编译期切换）。

## 快速开始

```powershell
# 需要：CMake ≥3.22、Ninja、arm-none-eabi-gcc（STM32CubeCLT）、Git
cmake --preset Debug
cmake --build --preset Debug

cmake --preset Release
cmake --build --preset Release
```

| 预设 | 说明 |
|---|---|
| `Debug` | `-Og -g3`，`.clangd` 指向 `build/Debug` |
| `Release` | `-O2`，提交与验收用 |
| `DebugHID` / `ReleaseHID` | 切到 Custom HID 传输层（T5b，需配合 CubeMX 改 HID 类） |

PC 端逻辑单元测试（不需要硬件）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/host_test/run_tests.ps1
```

## 硬件

| 项 | 型号 |
|---|---|
| MCU | STM32F401CCU6（Cortex-M4F，84MHz，256KB Flash / 64KB RAM） |
| 舵机 | SG90 ×4（50Hz，500~2500µs） |
| 姿态传感器 | MPU6050（GY-521，I²C 400kHz） |
| 调试器 | ST-Link V2 / DAPLink |

引脚与供电接线：**[docs/02-接线表.md](docs/02-接线表.md)**
CubeMX 逐项配置：**[docs/01-CubeMX配置清单.md](docs/01-CubeMX配置清单.md)**

## 目录结构

```
protocol/        帧编解码、CRC8          —— 无 HAL 依赖，可单测
components/      映射、插补、MPU6050     —— 无 HAL 依赖（除 mpu6050 用 bsp_i2c）
bsp/             PWM/I2C/USART/GPIO/USB —— 只碰 HAL，不含业务
application/     调度、状态机、协议应用   —— 业务逻辑
cmake/           app_sources.cmake       —— 登记新增源码（在根 CMakeLists 里 include）
docs/            00 执行手册 … 05 验收自检表
tools/host_test/ PC 端单测 + HAL 桩
```

## 协议摘要（详见 `protocol/arm_protocol.h`）

帧格式：`A5 | type | len | payload(N) | CRC8`，小端，CRC 多项式 0x07 初值 0x00。

| 类型 | 名称 | 方向 | 载荷 |
|---|---|---|---|
| 0x10 | 状态上报 | 设备→上位机 | 19 字节（4×实际 + 4×目标 + pitch + status） |
| 0x20 | 目标角度 | 上位机→设备 | 11 字节（4×目标 + duration + profile） |
| 0x21 | 模式切换 | 上位机→设备 | 1（0 手动 / 1 自动 / 2 回零 / 3 清除故障） |
| 0x22 | 急停 | 上位机→设备 | 1（1 = 急停） |
| 0x23 | 心跳 | 双向 | 1（原样回发） |
| 0x24 | 版本查询/应答 | 双向 | 2（0x01 0x00） |

> **任务书第 12 页示例帧有一处笔误**：`A5 21 01 02 82` 标注为"切换到自动"，
> 但按 4.3 定义表 `0x02` 是**回零**（自动是 `0x01`），且 CRC 与 `0x02` 一致。
> 本工程以定义表为准。上位机联调时若发 `0x02`，设备会执行回零，请注意。

## 状态位

| 位 | 含义 |
|---|---|
| bit0 | 1 = 自动 |
| bit1 | 1 = 正在回零 |
| bit2 | 1 = 故障 / 受限（急停、I2C 失效、目标被限位钳位） |
| bit3 | 1 = 正在运动 |
| bit4 | 1 = I2C 传感器失效 |
| bit5 | 1 = 指令或心跳超时 |
| bit6~7 | 保留 0 |

## 关键设计

- **同起同停**：所有轴共用一条归一化进度曲线 `s(t)`，`q_i = q_i0 + Δq_i·s`，
  同步是数学性质而不是逐轴调参；
- **不阻塞控制节拍**：USB 发送走环形缓冲（满则丢最旧），I²C 放在主循环；
- **传输层与协议分离**：`bsp/usb_link.h` 抽象，CDC/HID 编译期二选一，应用层零改动；
- **限位双保险**：`servo_map_clamp_angle()` 钳角度 + `servo_map_angle_to_us()` 钳脉宽。

## 状态机（T6）

```
MANUAL ──短按──► AUTO ──短按──► MANUAL
MANUAL/AUTO ──长按──► HOMING ──到位──► 返回进入前的模式
任意 ──0x22 急停──► FAULT ──0x21=3 或长按──► MANUAL
```

## 已知限制

1. 无编码器：堵转/撞限位后角度模型漂移，需重启回零；
2. 负载 >200g 时 SG90 跟随误差可能超过 1°；
3. 关节空间插补，末端轨迹非直线（考核不要求）。

## 提交物

| 项 | 位置 |
|---|---|
| 设计说明 | `docs/04-设计说明模板.md`（导出 PDF） |
| 执行手册 | `docs/00-执行手册.md` |
| 验收自检 | `docs/05-验收自检表.md` |
| AI 使用记录 | [AI.MD](AI.MD) |
| 演示视频 | `<填链接>` |
| 原理图 | `<填 PDF/链接>` |
