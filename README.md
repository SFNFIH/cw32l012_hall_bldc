# CW32L012 PWM + DMA — 呼吸灯

> 基于 **CW32L012 StartKit** 的 ATIM PWM 输出 + DMA 自动占空比更新示例，呼吸灯效果。

[![MCU](https://img.shields.io/badge/MCU-CW32L012x8-blue)](https://www.whxy.com)
[![Toolchain](https://img.shields.io/badge/Toolchain-GCC%20ARM-green)](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain)
[![Build](https://img.shields.io/badge/Build-CMake%20%2B%20Ninja-orange)](https://cmake.org/)

---

## 🧪 功能说明

| 项目 | 说明 |
|------|------|
| **PWM** | ATIM CH1 → PA05, 边沿对齐向上计数, 1 kHz |
| **占空比** | 64 点三角波查找表 (10% ~ 90%), 呼吸灯效果 |
| **DMA** | CH1 硬件触发 (ATIM 更新事件), 自动循环搬运占空比到 CCR1 |
| **CPU** | 完全释放, 无需软件干预占空比更新 |

### 硬件

- CW32L012 StartKit
- 示波器探头接 **PA05** 或观察板载 LED 呼吸效果

---

## 📂 项目结构

```
.
├── Board/               # 板级支持 (StartKit 引脚定义)
├── Libraries/           # CW32L012 标准外设库
│   ├── inc/             #   头文件
│   └── src/             #   驱动源码
├── USER/
│   ├── inc/             # 用户头文件 (main.h, interrupts)
│   └── src/             # 用户源码 (main.c, interrupts, SysTick)
├── cmake/               # CMake 工具链文件 (gcc-arm-none-eabi.cmake)
├── CMakeLists.txt       # 顶层 CMake 构建脚本
├── CMakePresets.json    # CMake 预设 (Debug / Release)
├── cw32l012_flash.ld    # 链接脚本
├── startup_cw32l012x8.s # 启动汇编
├── flash_cw32.py        # pyOCD 烧录脚本
├── pyocd.yml            # pyOCD 配置
└── cw32l012.svd         # CMSIS-SVD 调试描述文件
```

---

## 🔧 构建 & 烧录

### 前置条件

- `arm-none-eabi-gcc` (推荐 [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads))
- CMake ≥ 3.22
- Ninja (推荐) 或 MinGW Make
- OpenOCD (如需 `flash`/`debug-ocd` 目标) 或 pyOCD

### 构建

```bash
# Debug
cmake --preset Debug
cmake --build build/Debug

# Release
cmake --preset Release
cmake --build build/Release
```

构建产物位于 `build/<preset>/`:

| 文件 | 用途 |
|------|------|
| `cw32l012_pwm_dma.elf` | 调试 |
| `cw32l012_pwm_dma.hex` | 烧录 |
| `cw32l012_pwm_dma.bin` | 镜像 |

### 烧录

```bash
# OpenOCD (CMSIS-DAP)
cmake --build build/Debug -t flash

# OpenOCD (ST-Link)
cmake --build build/Debug -t flash-stlink

# pyOCD
python flash_cw32.py
```

### 调试

```bash
# 启动 OpenOCD server
cmake --build build/Debug -t debug-ocd

# 另一个终端连接 GDB
arm-none-eabi-gdb build/Debug/cw32l012_pwm_dma.elf \
    -ex "target extended-remote :3333" \
    -ex "monitor reset halt"
```

---

## ⚙️ 关键参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 系统时钟 HCLK | 8 MHz | HSI / 12 |
| PCLK | 1 MHz | HCLK / 8 |
| PWM 频率 | 1 kHz | PSC=0, ARR=999 |
| 占空比范围 | 10% ~ 90% | 100 ~ 900 / 999 |
| 查找表大小 | 64 点 | 三角波, 一个呼吸周期 ≈ 64 ms |

---

## 🧬 DMA 传输链

```
ATIM 更新事件 (每次溢出)
    │
    └──► DMA CH1 ────► duty_table[i] ──► ATIM CCR1
         (硬件触发)      (RAM, 自增)      (外设寄存器, 固定)

传输 64 次 → 自动重传 (RestartEnable) → 无限循环
```

---

## 📝 注意事项

- CW32L012 是 **Cortex-M0+**，`SCB->SHP` 寄存器不可写，因此 `InitTick()` 直接操作 SysTick 寄存器，不使用 `NVIC_SetPriority`
- 上电后 DMA 可能处于复位状态，使用前需 `__SYSCTRL_DMA_RST_DISABLE()`
- DMA 使用 **BLOCK 模式**（非 BULK），每个 ATIM 更新事件传输一个 CCR1 值

---

## 📄 License

MIT
