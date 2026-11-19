# 01 CubeMX 配置清单（STM32F401CCU6）

> 版本要求：STM32CubeMX **6.12 及以上**，Toolchain 选 **CMake**（任务书 2.1）。
> 打开 `New Project` → 搜 `STM32F401CC` → 选 `STM32F401CCUx`。

---

## 1 Pinout & Configuration

### 1.1 System Core

| 项目 | 设置 | 说明 |
|---|---|---|
| SYS → Debug | **Serial Wire** | 只留 SWD，释放 PB3/PB4（本工程没用到，但保留灵活性） |
| SYS → Timebase Source | SysTick | HAL 时基；控制节拍另用 TIM6 |
| RCC → HSE | **Crystal/Ceramic Resonator** | 板载 25MHz（若你的板是 8MHz，见 2.2） |
| RCC → LSE | Disable | 用不到 |
| GPIO → PC13 | Output Push Pull, Low speed, No pull | 板载 LED，低电平点亮 |
| GPIO → PB12 | Input, **Pull-up** | 外接按键到 GND，按下为低 |

### 1.2 Timers

**TIM3 —— 四路 50Hz 舵机 PWM**

| 参数 | 值 |
|---|---|
| Clock Source | Internal Clock |
| Channel1 | PWM Generation CH1（PB4） |
| Channel2 | PWM Generation CH2（PB5） |
| Channel3 | PWM Generation CH3（PB0） |
| Channel4 | PWM Generation CH4（PB1） |
| Prescaler | **83** |
| Counter Period (ARR) | **19999** |
| auto-reload preload | Enable |
| Pulse（四路初值） | **1500** |

推导：TIM3 时钟 84MHz → 84MHz/(83+1) = **1MHz**，即 1 tick = 1µs；
周期 (19999+1)µs = 20000µs = **50Hz**；分辨率 **1µs**（满足"优于 1µs"）。

> 若评审要求更细分辨率：PSC 改 41 → 2.38MHz（0.42µs/tick），ARR 改 47618。
> 此时 `bsp_pwm_set_pulse()` 内部要把 µs 换算成 tick，注释里有说明。

**TIM6 —— 1ms 控制节拍**

| 参数 | 值 |
|---|---|
| Activated | ✔ |
| Prescaler | 83 |
| Counter Period | 999 |
| auto-reload preload | Enable |
| NVIC → TIM6 global interrupt | **Enable**，Preemption Priority = 2 |

### 1.3 Connectivity

**USART1 —— 调试串口**

| 参数 | 值 |
|---|---|
| Mode | Asynchronous |
| Baud Rate | 115200 |
| Word Length | 8 Bits |
| Parity | None |
| Stop Bits | 1 |
| 引脚 | PA9 = TX, PA10 = RX |

**I2C1 —— MPU6050**

| 参数 | 值 |
|---|---|
| Mode | I2C |
| Speed Mode | **Fast Mode** |
| I2C Speed | **400000 Hz** |
| 引脚 | PB8 = SCL, PB9 = SDA |
| GPIO 上拉 | 内部上拉即可（模块自带 4.7k 时以模块为准） |

**USB_OTG_FS —— 设备模式**

| 参数 | 值 |
|---|---|
| Mode | **Device_Only** |
| Speed | Full Speed（12Mbit/s） |
| 引脚 | PA11 = DM, PA12 = DP |
| **Activate VBUS sensing** | **Disable**（关键！不禁用的话不接 VBUS 线时枚举失败） |
| Activate SOF | Disable |
| 其他 | 默认 |

> PA11/PA12 一旦给 USB，就**不能**再当 PWM 用。本工程舵机用 TIM3，避开了这两个引脚，
> 这正是不要用 TIM1_CH4(PA11) 的原因。

### 1.4 Middleware

**USB_DEVICE**

| 参数 | 值 |
|---|---|
| Class for FS IP | **Communication Device Class (Virtual Port Com)** |
| VID | 0x0483（ST 默认，**不得修改**） |
| PID | 0x5740（CDC 默认） |
| Product String | `ec-round2 CDC` |

生成后确认 `USB_DEVICE/App/usbd_desc.c` 里 `USBD_VID 0x0483`、`USBD_PID 0x5740`。

---

## 2 Clock Configuration

### 2.1 25MHz 晶振（WeAct F401CCU6 标配）

```
HSE = 25 MHz
PLL Source = HSE
PLLM = 25      -> 1 MHz 鉴相
PLLN = 336     -> VCO = 336 MHz
PLLP = 4       -> SYSCLK = 84 MHz
PLLQ = 7       -> USB = 48 MHz

SYSCLK 84 MHz
AHB  Prescaler = 1  -> HCLK 84 MHz
APB1 Prescaler = 2  -> PCLK1 42 MHz（TIM2~7 定时器时钟 = 84MHz）
APB2 Prescaler = 1  -> PCLK2 84 MHz
USB Prescaler = PLLQ -> 48 MHz
```

### 2.2 8MHz 晶振的板子

只改 `PLLM = 8`，其余（PLLN=336、PLLP=4、PLLQ=7）不变，结果同样是 84/48MHz。

> **判断方法**：看板上晶振丝印。25.000 就是 25MHz，8.000 就是 8MHz。
> 填错的后果：USB 枚举失败 + 串口波特率偏。这是新手最常见的坑。

### 2.3 检查点

- `SYSCLK = 84 MHz`、`HCLK = 84 MHz`、`PCLK1 = 42 MHz`、`PCLK2 = 84 MHz`；
- **USB clock = 48 MHz**（必须精确，否则枚举不了）。

---

## 3 Project Manager

| 项目 | 值 |
|---|---|
| Project Name | `ec-round2` |
| Project Location | 本仓库根目录 |
| Toolchain / IDE | **CMake** |
| Application Structure | Advanced（默认） |
| Minimum Heap Size | 0x200 |
| Minimum Stack Size | 0x800 |
| **Generate peripheral initialization as a pair of .c/.h files** | ✔ |
| Keep User Code when re-generating | ✔ |
| Delete previously generated files when not re-generated | ✔ |
| Generate .ioc for toolchain | ✔ |

> Stack 给到 0x800 是因为 `printf`/`vsnprintf` 在自检里用得比较多，
> 默认 0x400 遇到浮点格式化容易栈溢出（表现为莫名其妙进 HardFault）。

---

## 4 生成后必须做的 3 件事

1. **根 `CMakeLists.txt` 加一行**（放在 `target_sources` / `target_include_directories` 之后）：

```cmake
include(${CMAKE_CURRENT_LIST_DIR}/cmake/app_sources.cmake)
```

2. **确认 `cmake/app_sources.cmake` 里的路径存在**（仓库已带）。
3. **编译**：

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

> 如果 CubeMX 覆盖了 `CMakePresets.json`，用仓库里这份替换（已定义 Debug/Release）。

---

## 5 附录 A：Custom HID 的报告描述符（T5b）

CubeMX 里把 Class 改成 `Custom Human Interface Device` 后，`usbd_hid.c` 里会有一个
鼠标用的 4 字节描述符，整段替换为：

```c
__ALIGN_BEGIN static uint8_t HID_ReportDesc[USB_HID_CONFIG_DESC_SIZ] __ALIGN_END =
{
    0x06, 0x00, 0xFF,        /* Usage Page (Vendor Defined 0xFF00) */
    0x09, 0x01,              /* Usage (0x01)                      */
    0xA1, 0x01,              /* Collection (Application)           */

    0x09, 0x02,              /*   Usage (0x02)                     */
    0x15, 0x00,              /*   Logical Minimum (0)              */
    0x26, 0xFF, 0x00,        /*   Logical Maximum (255)            */
    0x75, 0x08,              /*   Report Size (8)                  */
    0x95, 0x40,              /*   Report Count (64)                */
    0x81, 0x02,              /*   Input (Data,Var,Abs)             */

    0x09, 0x03,              /*   Usage (0x03)                     */
    0x15, 0x00,              /*   Logical Minimum (0)              */
    0x26, 0xFF, 0x00,        /*   Logical Maximum (255)            */
    0x75, 0x08,              /*   Report Size (8)                  */
    0x95, 0x40,              /*   Report Count (64)                */
    0x91, 0x02,              /*   Output (Data,Var,Abs)            */

    0xC0                     /* End Collection                     */
};
```

要点：
- **不要**加 `Report ID` 条目（报告 ID = 0 就是"无 ID 前缀"）；
- `USB_HID_CONFIG_DESC_SIZ` 要跟着改（在 `usbd_hid.h`）；
- `usbd_desc.c` 的 `USBD_PID` 改成 **0x0123**；
- 收到 OUT 报告时在 `usbd_hid.c` 的 `USBD_HID_Setup`（`HID_REQ_SET_REPORT` 分支）
  或 `OutEvent` 回调里调 `bsp_usb_hid_on_rx(Report, 64)`；
- 发送完成回调里调 `bsp_usb_hid_on_tx_complete()`。

---

## 6 附录 B：本工程的依赖关系（评审问"为什么这么分层"时用）

```
application/app_main.c          调度：1ms 标志 -> 200Hz 控制 / 50Hz 采样 / 30Hz 上报
   ├─ app_motion.c  ── components/motion_profile.c     纯算法，可单测
   ├─ app_servo.c   ── components/servo_map.c          纯算法，可单测
   │                 └─ bsp_pwm.c                      只写 CCR
   ├─ app_protocol.c ─ protocol/arm_protocol.c + crc8.c 纯算法，可单测
   │                 └─ bsp/usb_link.c → CDC 或 HID
   ├─ app_state.c                                      四态机
   └─ components/mpu6050.c ─ bsp_i2c.c
```

**依赖方向永远向下**：`components/` 不知道 HAL，`protocol/` 不知道 USB，
所以这三层能在 PC 上用 gcc 直接跑单测（`tools/host_test`）。
这句话就是 G3"代码组织"的答辩答案。
