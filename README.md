# CW32L012 无感六步 BLDC 驱动

基于 **CW32L012** 的无感 BLDC 六步换相：三相桥臂反电势过零 + ATIM 三相互补 PWM，按键启停、电位器调速。

[![MCU](https://img.shields.io/badge/MCU-CW32L012x8-blue)](https://www.whxy.com)
[![Toolchain](https://img.shields.io/badge/Toolchain-GCC%20ARM-green)](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain)
[![Build](https://img.shields.io/badge/Build-CMake%20%2B%20Ninja-orange)](https://cmake.org/)

仓库：https://github.com/SFNFIH/cw32l012_hall_bldc

---

## 功能说明

| 模块 | 说明 |
|------|------|
| **启动** | 对齐（固定一步约 200 ms）→ 开环强拖升频 → 反电势过零切入闭环 |
| **换相** | ATIM 20 kHz 中断采样浮空相反电势；过零后再延时约 30° 换相 |
| **PWM** | ATIM 中央对齐互补 PWM，约 20 kHz（PCLK=8 MHz，ARR=199） |
| **下桥** | 导通下桥相：`CCxE+CCxNE` + `FORCED_INACTIVE` |
| **调速** | PA07 ADC 电位器 → 占空比（下限 36，上限 170；开环起步约 50） |
| **启停** | PC13 按键切换运行；LED(PB09) 指示运行状态 |
| **调试串口** | UART1 @ 115200（PC14/PC15，`CR2.SWAP`） |

霍尔接口仍初始化，仅作对比观测（`hall=`），**不参与换相**。

### 引脚

| 信号 | 引脚 | 说明 |
|------|------|------|
| 电机 U/V/W 上桥 | PA08 / PA09 / PA10 | ATIM CH1/2/3 |
| 电机 U/V/W 下桥 | PB13 / PB14 / PB15 | ATIM CH1N/2N/3N |
| 反电势 U/V/W | PA03 / PA04 / PA05 | ADC1 IN3/4/5，相电压 10k/1k 分压 + 470 pF |
| 母线 V12 | PA06 | ADC1 IN6，3.3k/1k 分压，用于过零中点 |
| 调速电位器 | PA07 | ADC1 IN7 |
| 启停按键 | PC13 | 按下=高，内部下拉 |
| 运行指示灯 | PB09 | 高电平点亮 |
| 调试 UART | PC14 / PC15 | 经 DAPLink VCP（常用 COM5） |

PCB 桥臂与 MCU 通道对应：PA08→HIN3→`OUT_W`，PA09→HIN2→`OUT_V`，PA10→HIN1→`OUT_U`。浮空相采样按此映射。

### 六步表（与有感正向表相同）

正向序列：`001 → 011 → 010 → 110 → 100 → 101 → 001`  
每步两相导通（一相互补 PWM，一相下桥常开），第三相浮空并检测反电势过零。

---

## 项目结构

```
.
├── BSP/                 # 板级驱动
│   ├── BSP_motor.*      # ATIM 六步换相 / 互补 PWM
│   ├── BSP_bemf.*       # 反电势过零无感状态机
│   ├── BSP_halltim.*    # 霍尔接口（观测，不换相）
│   ├── BSP_adc.*        # 反电势 / 母线 / 调速
│   ├── BSP_key.*        # PC13 + BTIM1 扫描
│   ├── BSP_led.*        # PB09
│   └── BSP_usart.*      # UART1 调试口
├── USER/
│   ├── inc/             # main.h, interrupts
│   └── src/             # main, 中断, 时钟 override
├── Libraries/           # CW32L012 标准外设库
├── Board/               # 板级定义
├── cmake/               # gcc-arm-none-eabi 工具链
├── Doc/原理图.net      # 网表（分压与相节点）
├── flash_cw32.py        # pyOCD 烧录
├── capture_uart.py      # 串口抓取调试行
├── CMakeLists.txt
└── ...
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

1. 接线：三相桥臂、反电势分压（板载）、电位器与按键。霍尔可悬空。
2. 烧录后复位；上电默认 **停机**。
3. **PC13** 按一下启动（LED 亮）：先对齐，再开环加速，过零稳定后进入 `RUN`。
4. **PA07** 在闭环后调节转速；建议中速起步。
5. 过零长期丢失会进入 `FAULT`（电机关断），再按键重试。
6. 串口 115200 查看：

```
DBG st=RUN step=3 hall=4 bemf=512 mid=677 duty=50 zc=120 miss=0 dt=40
```

```bash
python capture_uart.py COM5 115200 20
```

---

## 关键参数

| 参数 | 值 | 说明 |
|------|-----|------|
| HCLK / PCLK | 8 MHz | HSI / 12，PCLK 不分频 |
| PWM | ≈ 20 kHz | 中央对齐，ARR=199，死区 ≈ 2 µs |
| 反电势分压 | 1/11 | 10 kΩ / 1 kΩ，470 pF 到地 |
| 母线分压 | ≈ 1/4.3 | 3.3 kΩ / 1 kΩ，过零中点 = Vbus/2 |
| 开环斜坡 | 40 ms → 4 ms/步 | 对齐 200 ms 后强拖 |
| 占空比 | 36 ~ 170 | 相对 ARR；无感下限略高于有感 |
| UART | 115200 8N1 | 调试打印 |

---

## 注意事项

- CW32L012 为 **Cortex-M0+**，`InitTick()` 直接配置 SysTick，不写不可用的 `SCB->SHP`。
- 下桥必须 **双使能 + Forced Inactive**，仅 `CCxNE=1` 时 CHN 可能一直为低。
- 过零极性与浮空相映射在 `BSP_bemf.c` 的 `s_float_ph` / `s_zc_rise` / `s_bemf_ch`。若无力矩或反转，先核对接线再改这两张表。
- 低速反电势弱，闭环转速不宜过低；堵转或失步会 `FAULT`。

---

## License

MIT
