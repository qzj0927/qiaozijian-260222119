# ec-round2-260222119

CVIR 机甲大师 2026 电控组二轮考核 —— 四自由度舵机机械臂（底座 / 肩 / 肘 / 夹爪）

- **MCU**：STM32F401CCU6（Cortex-M4F，84MHz）
- **技术主线**：STM32 + 4×PWM + I²C(MPU6050) + USB CDC/Custom HID + CMake
- **状态**：工程从零编写中

> 早期由 AI 生成的参考骨架已清空，仅保留文档。
> 需要参考时可用 `git checkout skeleton-v1 -- .` 取回（tag 已推送到本仓库）。

## 构建

```powershell
# 待补：CubeMX 生成工程后填写
cmake --preset Debug
cmake --build --preset Debug
```

## 待补充（提交前必须写全，任务书 5.1）

- [ ] 构建预设（Debug / Release）与一次干净编译的命令
- [ ] 所选 MCU 与工具链版本（CubeMX / arm-none-eabi-gcc / CMake）
- [ ] 接线表（引脚分配 + 供电拓扑）
- [ ] 协议实现说明（帧格式、状态位、超时处理）
- [ ] 演示视频链接
- [ ] AI 使用记录：[AI.MD](AI.MD)

## 文档

| 文件 | 用途 |
|---|---|
| [docs/00-执行手册.md](docs/00-执行手册.md) | 九步执行流程、每步完成标志、常见坑 |
| [docs/01-CubeMX配置清单.md](docs/01-CubeMX配置清单.md) | 时钟树、TIM3/TIM6、USART1、I2C1、USB 逐项配置 |
| [docs/02-接线表.md](docs/02-接线表.md) | 引脚分配、供电接线、上电顺序 |
| [docs/03-供电与接口安全检查表.md](docs/03-供电与接口安全检查表.md) | 任务书 1.5 要求的安全检查表（可直接填） |
| [docs/04-设计说明模板.md](docs/04-设计说明模板.md) | 提交用设计说明（1~2 页） |
| [docs/05-验收自检表.md](docs/05-验收自检表.md) | 按任务书附件二逐条自测 |
