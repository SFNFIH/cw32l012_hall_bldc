# CW32L012 无感 FOC（高频注入）

基于 **CW32L012** 的 PMSM/BLDC **磁场定向控制**：三相互补 SVPWM、Id/Iq 电流环，d 轴 **10 kHz 方波高频注入** + PLL 估角。

[![MCU](https://img.shields.io/badge/MCU-CW32L012x8-blue)](https://www.whxy.com)
[![Toolchain](https://img.shields.io/badge/Toolchain-GCC%20ARM-green)](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain)
[![Build](https://img.shields.io/badge/Build-CMake%20%2B%20Ninja-orange)](https://cmake.org/)

仓库：https://github.com/SFNFIH/cw32l012_hall_bldc

相对六步 HFI：三相不再浮空，全程正弦调制；位置来自注入电流的 q 轴分量，而不是反电势。

---

## 功能说明

| 模块 | 说明 |
|------|------|
| **ALIGN** | Ud 固定、θ=0，约 200 ms 把转子拉到 d 轴 |
| **I-F** | 电流闭环 + 开环升频（θ 斜坡），同时开 HFI PLL |
| **RUN** | θ 改由 PLL；电位器给定 **Iq** |
| **HFI** | 每个 FOC 周期翻转 d 轴注入电压；`sign·Iq` 低通后作位置误差 |
| **采样** | PWM **峰值**（下桥全开）采 PA00/01/02 三电阻 |
| **PWM** | ATIM 中央对齐三相互补，20 kHz，死区约 2 µs |
| **启停** | PC13；LED PB09 |

霍尔只观测，不参与控制。

### 引脚

| 信号 | 引脚 | 说明 |
|------|------|------|
| 相 W/V/U 上桥 | PA08 / PA09 / PA10 | ATIM CH1 / CH2 / CH3 |
| 相 W/V/U 下桥 | PB13 / PB14 / PB15 | ATIM CH1N / CH2N / CH3N |
| 电流 U/V/W | PA00 / PA01 / PA02 | INA180 + 下桥分流 |
| 调速 | PA07 | 映射为 Iq 给定 |
| 按键 | PC13 | 启停 |
| UART | PC14 / PC15 | 115200 |

PCB：PA08→`OUT_W`，PA09→`OUT_V`，PA10→`OUT_U`。

---

## 构建与烧录

```bash
cmake --preset Debug
cmake --build build/Debug
python flash_cw32.py build/Debug/cw32l012_hall_bldc.elf
```

---

## 使用方法

1. 接三相桥臂与分流。霍尔可悬空。
2. **PC13** 启动：`ALIGN` → `IF` → `RUN`。
3. **PA07** 在 `RUN` 后调 Iq（转矩/转速）。
4. 过流会 `FAULT`，再按键重试。
5. 串口：

```
DBG st=RUN th=80 hall=5 id=12 iq=260 iqref=280 dem=8 w=35
```

`th` 为电角度高 8 位（0–255≈0–360°），`dem` 为 HFI 解调，`w` 为 PLL 角速度。

---

## 注意

- MCU 仅 **8 MHz Cortex-M0+**，FOC 在 20 kHz 峰值中断里用 Q15 整数；PI / `HFI_V` / `HFI_KP` 需按电机微调（`BSP_foc.c`）。
- 板载 **INA180 单向**，负电流会削顶，FOC 精度受此限制。
- 表贴磁钢凸极弱时 PLL 可能锁不稳，电机会停在 I-F 开环角；可加大 `HFI_V` 或加长 `IF_TICKS`。
- 下桥必须互补 PWM；峰值采样对应 PWM1 的关断中心。

---

## License

MIT
