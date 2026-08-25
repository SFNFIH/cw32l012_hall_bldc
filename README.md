# CW32L012 高频注入无感六步 BLDC

基于 **CW32L012** 的无感 BLDC：在六步互补 PWM 上叠加 **10 kHz 方波注入**，用下桥分流电流的 `|ΔI|` 观测凸极/饱和，静止辨识扇区后换相。

[![MCU](https://img.shields.io/badge/MCU-CW32L012x8-blue)](https://www.whxy.com)
[![Toolchain](https://img.shields.io/badge/Toolchain-GCC%20ARM-green)](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain)
[![Build](https://img.shields.io/badge/Build-CMake%20%2B%20Ninja-orange)](https://cmake.org/)

仓库：https://github.com/SFNFIH/cw32l012_hall_bldc

本分支相对反电势过零分支：位置来自 **高频电流响应**，不依赖反电势幅值，低速可用。

---

## 功能说明

| 模块 | 说明 |
|------|------|
| **IDENT** | 六个电压矢量各注入约 1.2 ms 直流脉冲，比较三相 INA180 电流；最大电流方向 ≈ d 轴（饱和） |
| **注入** | 占空比每 PWM 周期翻转 `±12`（相对 ARR=199），等效 **10 kHz** 方波 |
| **观测** | 采样当前步 **下桥常开相** 分流；`|I[k]-I[k-1]|` 低通后与 `1/L` 成正比 |
| **换相** | 扇区内 `|ΔI|` 相对进入值上升（L 下降、转子靠近该矢量 d 轴）则进入下一步 |
| **启动** | IDENT → 开环斜坡（同时注入）→ 凸极信号稳定后 `RUN` |
| **PWM** | ATIM 中央对齐互补，约 20 kHz |
| **调速** | PA07 电位器（下限 40，上限 170） |
| **启停** | PC13；LED(PB09) |

霍尔只观测、不换相。

### 引脚

| 信号 | 引脚 | 说明 |
|------|------|------|
| 电机 U/V/W 上桥 | PA08 / PA09 / PA10 | ATIM CH1/2/3 |
| 电机 U/V/W 下桥 | PB13 / PB14 / PB15 | ATIM CH1N/2N/3N |
| 相电流 U/V/W | PA00 / PA01 / PA02 | ADC1 IN0/1/2，INA180 + 下桥分流 |
| 反电势 U/V/W | PA03 / PA04 / PA05 | 本分支不用于换相 |
| 调速电位器 | PA07 | ADC1 IN7 |
| 启停按键 | PC13 | 按下=高 |
| 运行指示灯 | PB09 | 高电平点亮 |
| 调试 UART | PC14 / PC15 | 115200 |

固件相 CH1/2/3 = PA08/09/10 = PCB `OUT_W/V/U`，电流通道按此映射。

---

## 构建与烧录

```bash
cmake --preset Debug
cmake --build build/Debug
python flash_cw32.py build/Debug/cw32l012_hall_bldc.elf
```

---

## 使用方法

1. 接好三相桥臂与分流（板载 INA180）。霍尔可悬空。
2. 复位后停机；**PC13** 启动：先 ident（电机可能微动），再斜坡，最后 `RUN`。
3. 闭环后用 **PA07** 调速。
4. 凸极信号长期对不上会 `FAULT`，再按键重试。
5. 串口：

```
DBG st=RUN step=2 hall=2 ident=4 amp=28 i=610 duty=52 lock=40 miss=0 dt=88
```

`amp` 为高频电流差幅值；`ident` 为静止辨识到的 d 轴步号。

---

## 关键参数（`BSP_hfi.c`）

| 参数 | 值 | 说明 |
|------|-----|------|
| PWM | 20 kHz | 中央对齐，ARR=199 |
| 注入 | ±12 duty | 每周期翻转 → 10 kHz |
| IDENT | 48 duty × 1.2 ms × 6 步 | 步间浮空 0.5 ms |
| 斜坡 | 35 ms → 4.5 ms/步 | 与注入同时进行 |
| 占空比 | 40 ~ 170 | 需留出 ±12 注入余量 |

表贴磁钢凸极弱时，IDENT 可能不准，斜坡仍会强拖；可加大 `HFI_AMP` 或 `HFI_IDENT_DUTY`。

---

## License

MIT
