# CW32L012 霍尔六步 BLDC 驱动

基于 **CW32L012** 的有感 BLDC 六步换相示例：HALLTIM 霍尔接口 + ATIM 三相互补 PWM，按键启停、电位器调速。

[![MCU](https://img.shields.io/badge/MCU-CW32L012x8-blue)](https://www.whxy.com)
[![Toolchain](https://img.shields.io/badge/Toolchain-GCC%20ARM-green)](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain)
[![Build](https://img.shields.io/badge/Build-CMake%20%2B%20Ninja-orange)](https://cmake.org/)

仓库：https://github.com/SFNFIH/cw32l012_hall_bldc  
里程碑标签：`电机成功转动`

---

## 功能说明

| 模块 | 说明 |
|------|------|
| **换相** | HALLTIM 霍尔变化中断触发六步换相；换相后短空白忽略 1 次边沿，抑制抖边 |
| **PWM** | ATIM 中央对齐互补 PWM，约 20 kHz（PCLK=8 MHz，ARR=199） |
| **下桥** | 导通下桥相：`CCxE+CCxNE` + `FORCED_INACTIVE`（仅开 `CCxNE` 时实测无输出） |
| **调速** | PA07 ADC 电位器 → 占空比（下限 16，上限 170） |
| **启停** | PC13 按键切换运行；LED(PB09) 指示运行状态 |
| **调试串口** | UART1 @ 115200（PC14/PC15，`CR2.SWAP`），打印霍尔/占空比等 |

### 引脚

| 信号 | 引脚 | 说明 |
|------|------|------|
| 电机 U/V/W 上桥 | PA08 / PA09 / PA10 | ATIM CH1/2/3 |
| 电机 U/V/W 下桥 | PB13 / PB14 / PB15 | ATIM CH1N/2N/3N |
| 霍尔 H1/H2/H3 | PB02 / PB10 / PB11 | HALLTIM CH1/2/3 |
| 调速电位器 | PA07 | ADC1 IN7 |
| 启停按键 | PC13 | 按下=高，内部下拉 |
| 运行指示灯 | PB09 | 高电平点亮 |
| 调试 UART | PC14 / PC15 | 经 DAPLink VCP（常用 COM5） |

### 六步表（霍尔 bit0=H1, bit1=H2, bit2=H3）

正向序列：`001 → 011 → 010 → 110 → 100 → 101 → 001`  
每步两相导通（一相互补 PWM，一相下桥常开），第三相浮空。

---

## 项目结构

```
.
├── BSP/                 # 板级驱动
│   ├── BSP_motor.*      # ATIM 六步换相 / 互补 PWM
│   ├── BSP_halltim.*    # 霍尔接口与 IRQ 换相
│   ├── BSP_adc.*        # PA07 调速采样
│   ├── BSP_key.*        # PC13 + BTIM1 扫描
│   ├── BSP_led.*        # PB09
│   └── BSP_usart.*      # UART1 调试口
├── USER/
│   ├── inc/             # main.h, interrupts
│   └── src/             # main, 中断, 时钟 override
├── Libraries/           # CW32L012 标准外设库
├── Board/               # 板级定义
├── cmake/               # gcc-arm-none-eabi 工具链
├── flash_cw32.py        # pyOCD 烧录
├── capture_uart.py      # 串口抓取调试行
├── debug_probe_log.py   # SWD 读调试邮箱
├── dump_atim.py         # 读 ATIM 寄存器
├── CMakeLists.txt
├── CMakePresets.json
├── cw32l012_flash.ld
└── startup_cw32l012x8.s
```

---

## 构建与烧录

### 前置条件

- `arm-none-eabi-gcc`
- CMake ≥ 3.22、Ninja
- pyOCD（推荐）或 OpenOCD；CMSIS-DAP / DAPLink

### 构建

```bash
cmake --preset Debug
cmake --build build/Debug
```

产物在 `build/Debug/`：

| 文件 | 用途 |
|------|------|
| `cw32l012_hall_bldc.elf` | 调试 |
| `cw32l012_hall_bldc.hex` / `.bin` | 烧录 |

### 烧录

```bash
# 推荐：指定 ELF
python flash_cw32.py build/Debug/cw32l012_hall_bldc.elf

# 或 OpenOCD 目标（需本机已配置 OPENOCD 路径）
cmake --build build/Debug -t flash
```

烧录时请关闭占用 DAPLink 串口的程序，避免 SWD 抢占失败。

---

## 使用方法

1. 接线按上表连接电机桥臂、霍尔、电位器与按键。
2. 烧录后复位；上电默认 **停机**。
3. **PC13** 按一下启动（LED 亮），再按停止。
4. **PA07** 调节转速；建议先中速，再慢慢升高。
5. 可选：串口 115200 查看 `DBG hall=... duty=...` 行。

```bash
python capture_uart.py COM5 115200 20
```

---

## 关键参数

| 参数 | 值 | 说明 |
|------|-----|------|
| HCLK / PCLK | 8 MHz | HSI / 12，PCLK 不分频 |
| PWM | ≈ 20 kHz | 中央对齐，ARR=199，死区 ≈ 2 µs |
| 霍尔滤波 FLT2 | ≈ 250 µs | PCLK=8 MHz，`FLT2LEN=2000` |
| 占空比 | 16 ~ 170 | 相对 ARR，避免顶满常通 |
| UART | 115200 8N1 | 调试打印 |

---

## 注意事项

- CW32L012 为 **Cortex-M0+**，`InitTick()` 直接配置 SysTick，不写不可用的 `SCB->SHP`。
- 下桥必须 **双使能 + Forced Inactive**，仅 `CCxNE=1` 时 CHN 可能一直为低。
- 霍尔顺序若与电机绕组不匹配，会出现无力矩或反转，需核对接线或调整 `s_step_table`。
- 高速边界抖边可能导致来回换向；当前用换相后忽略 1 次边沿缓解，可按实机再调滤波/空白。

---

## License

MIT
