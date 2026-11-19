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

