/**
 * @file    main.c
 * @author  damo
 * @brief   ATIM PWM + DMA 自动更新占空比
 *          - PWM: ATIM CH1 (PA05), 1kHz, 边沿对齐向上计数
 *          - DMA: CH1, ATIM更新事件硬件触发, 自动循环更新CCR1
 *          - 占空比表: 64点三角波, 实现呼吸灯效果
 *          - CPU完全解放, 无需软件干预占空比更新
 */
/******************************************************************************
 * Include files
 ******************************************************************************/
#include "../inc/main.h"

/******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
/** DMA传输完成状态标志 */
#define DMA_TC_FLAG     (1UL << 0)
/** 占空比查找表大小 */
#define DUTY_TABLE_SIZE 64

/******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/
volatile uint32_t g_dma_tc_count = 0;   // DMA传输完成次数计数

/******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/
static void SYSCTRL_Configuration(void);
static void GPIO_Configuration(void);
static void ATIM_Configuration(void);
static void DMA_Configuration(void);
static void NVIC_Configuration(void);
void ATIM_IRQHandlerCallBack(void);
void DMACH12_IRQHandlerCallBack(void);

/******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/

/**
 * @brief  PWM占空比查找表 (64点三角波/呼吸灯效果)
 * @note   值范围 100~900, 对应 10%~90% 占空比 (ARR=999)
 *         DMA自动将此表循环传输到ATIM CCR1寄存器
 *         在1kHz PWM频率下, 一个完整呼吸周期 = 64ms
 */
static const uint16_t duty_table[DUTY_TABLE_SIZE] = {
    100, 125, 150, 175, 200, 225, 250, 275,
    300, 325, 350, 375, 400, 425, 450, 475,
    500, 525, 550, 575, 600, 625, 650, 675,
    700, 725, 750, 775, 800, 825, 850, 875,
    900, 875, 850, 825, 800, 775, 750, 725,
    700, 675, 650, 625, 600, 575, 550, 525,
    500, 475, 450, 425, 400, 375, 350, 325,
    300, 275, 250, 225, 200, 175, 150, 125,
};

/******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/

/*****************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/

/**
 * @brief  主函数 — ATIM PWM + DMA 自动占空比更新
 *         PWM输出: PA05, 1kHz
 *         DMA自动将占空比表循环写入CCR1, CPU完全释放
 *         主循环仅做LED闪烁指示系统运行
 * @author damo
 * @return int32_t
 */
/**
  * @brief  覆盖库中的 InitTick() — 避免 NVIC_SetPriority 导致 HardFault
  * @note   CW32L012 Cortex-M0+ 的 SCB->SHP 寄存器不可写,
  *         NVIC_SetPriority(SysTick_IRQn) 会触发 HardFault
  *         因此直接操作 SysTick 寄存器, 不设置优先级
  */
void InitTick(uint32_t HclkFreq)
{
    /* 配置 SysTick 每 1ms 中断一次 */
    SysTick->LOAD = (HclkFreq / 1000U) - 1U;
    SysTick->VAL  = 0U;
    /* 使能 SysTick: 使用内核时钟, 使能中断, 使能计数器 */
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;
    /* 注意: 不调用 NVIC_SetPriority, CW32L012 的 SHP 寄存器不可写 */
}

int main(void)
{
    /* 系统时钟配置: HSI / 12 = 8MHz, PCLK = 8MHz / 8 = 1MHz */
    SYSCTRL_Configuration();

    /* SysTick初始化: 1ms tick (自定义版本, 避免NVIC_SetPriority HardFault) */
    InitTick(8000000);

    /* LED初始化: 直接操作GPIOB寄存器 (跳过有问题的GPIO_Init/Bsp_Init) */
    /* LED1=PB09, LED2=PB08, 高电平=灭 */
    __SYSCTRL_GPIOB_CLK_ENABLE();
    CW_GPIOB->ANALOG &= ~(GPIO_PIN_9 | GPIO_PIN_8);
    CW_GPIOB->DIR |= (GPIO_PIN_9 | GPIO_PIN_8);
    CW_GPIOB->ODR |= (GPIO_PIN_9 | GPIO_PIN_8);

    /* GPIO配置: PA05 复用为 ATIM CH1 */
    GPIO_Configuration();

    /* DMA配置: 必须在ATIM启动前配置 */
    DMA_Configuration();

    /* NVIC配置: 使能DMA中断 */
    NVIC_Configuration();

    /* ATIM PWM配置: 1kHz, 启动定时器 */
    ATIM_Configuration();

    while (1)
    {
        /* LED闪烁: 直接操作GPIOB TOG */
        CW_GPIOB->TOG |= (GPIO_PIN_9 | GPIO_PIN_8);
        SysTickDelay(500);    // 500ms
    }
}

/**
  * @brief  系统时钟配置
  *         HSI / 12 = 8MHz 系统时钟
  *         PCLK = HCLK / 8 = 1MHz
  * @param  None
  * @retval None
  */
static void SYSCTRL_Configuration(void)
{
    SYSCTRL_HSI_Enable(SYSCTRL_HSIOSC_DIV12);    // 配置系统时钟为8MHz
    SYSCTRL_PCLKPRS_Config(SYSCTRL_PCLK_DIV8);   // PCLK = 8MHz / 8 = 1MHz

    /* 使能外设时钟 */
    __SYSCTRL_ATIM_CLK_ENABLE();                 // ATIM时钟
    __SYSCTRL_GPIOA_CLK_ENABLE();                // GPIOA时钟 (PA05)
    __SYSCTRL_GPIOB_CLK_ENABLE();                // GPIOB时钟 (LED)
    __SYSCTRL_DMA_CLK_ENABLE();                  // DMA时钟
}

/**
  * @brief  GPIO配置: PA05 复用为 ATIM CH1 PWM输出
  * @param  None
  * @retval None
  */
static void GPIO_Configuration(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    /* PA05 复用为 ATIM CH1 */
    PA05_AFx_ATIMCH1();
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pins = GPIO_PIN_5;
    GPIO_Init(CW_GPIOA, &GPIO_InitStruct);
}

/**
  * @brief  ATIM PWM配置
  *         边沿对齐, 向上计数, 1kHz PWM频率
  *         CH1: PWM模式1, 高电平有效
  *         DMA将在每个更新事件自动更新CCR1
  * @param  None
  * @retval None
  */
static void ATIM_Configuration(void)
{
    ATIM_InitTypeDef ATIM_InitStruct = {DISABLE, 0};
    ATIM_OCInitTypeDef ATIM_OCInitStruct;

    /* --- 时基配置 --- */
    ATIM_InitStruct.BufferState = ENABLE;                       // 使能缓存寄存器
    ATIM_InitStruct.CounterAlignedMode = ATIM_COUNT_ALIGN_MODE_EDGE;  // 边沿对齐
    ATIM_InitStruct.CounterDirection = ATIM_COUNTING_UP;        // 向上计数
    ATIM_InitStruct.CounterOPMode = ATIM_OP_MODE_REPETITIVE;    // 连续运行模式
    ATIM_InitStruct.Prescaler = 0;                              // PSC=0, 1分频
    ATIM_InitStruct.ReloadValue = 999;                          // ARR=999, PWM频率=1MHz/(0+1)/(999+1)=1kHz
    ATIM_InitStruct.RepetitionCounter = 0;
    ATIM_Init(&ATIM_InitStruct);

    /* --- CH1输出比较配置 --- */
    ATIM_OCInitStruct.BufferState = ENABLE;                     // 使能预装载(CCR1缓冲)
    ATIM_OCInitStruct.OCComplement = DISABLE;
    ATIM_OCInitStruct.OCFastMode = ATIM_OC_FAST_MODE_DISABLE;
    ATIM_OCInitStruct.OCInterruptState = DISABLE;               // 不使用比较中断(DMA负责更新)
    ATIM_OCInitStruct.OCMode = ATIM_OCMODE_PWM1;                // PWM模式1
    ATIM_OCInitStruct.OCPolarity = ATIM_OCPOLARITY_NONINVERT;   // 高电平有效
    ATIM_OC1Init(&ATIM_OCInitStruct);

    /* 设置初始占空比 (DMA将在第一个更新事件后覆盖此值) */
    ATIM_SetCompare1(duty_table[0]);

    /* 使能CH1输出 */
    ATIM_CH1Config(ENABLE);

    /* 使能 ATIM 更新 DMA 请求 (UDE bit 8) — DMA需要此位才能接收触发 */
    CW_ATIM->IER |= (1UL << 8);  // UDE: Update DMA request Enable

    /* 启动ATIM定时器 和 PWM输出 */
    ATIM_Cmd(ENABLE);
    ATIM_CtrlPWMOutputs(ENABLE);
}

/**
  * @brief  DMA配置
  *         DMA CH1: ATIM更新事件触发 → 自动传输占空比表到CCR1
  *         源地址: duty_table (RAM, 自增)
  *         目标地址: CW_ATIM->CCR1 (外设寄存器, 固定)
  *         自动重传: 使能 (无限循环)
  * @param  None
  * @retval None
  */
static void DMA_Configuration(void)
{
    DMA_InitTypeDef DMA_InitStruct;

    /* DMA 复位释放: CW32L012 上电后 DMA 可能处于复位状态 */
    __SYSCTRL_DMA_CLK_ENABLE();
    __SYSCTRL_DMA_RST_DISABLE();  // 释放 DMA 复位 (AHBRST bit 1=1 表示不复位)

    /* --- DMA CH1 初始化 --- */
    /* 硬件BLOCK模式: 每个ATIM更新事件触发传输1个数据块(CCR1值)
     * BULK模式会一次性传完所有数据, 不适合PWM逐周期更新 */
    DMA_InitStruct.RestartEnable = TRUE;                        // 自动重传(循环)
    DMA_InitStruct.DataSize = DMA_DATA_SIZE_16BITS;             // 16位传输(CCR1为16位)
    DMA_InitStruct.SrcIncrement = DMA_ADDRESS_INCREMENT;        // 源地址自增(遍历数组)
    DMA_InitStruct.DstIncrement = DMA_ADDRESS_FIXED;            // 目标地址固定(CCR1寄存器)
    DMA_InitStruct.TransferMode = DMA_MODE_BLOCK;               // 块模式: 每触发→1个数据块
    DMA_InitStruct.TransferCount = DUTY_TABLE_SIZE;             // 共传输64个数据块
    DMA_InitStruct.BlockCount = 1;                              // 每块1个数据 (DMA_CNTy.REPEAT=1)
    DMA_InitStruct.SrcAddress = (uint32_t)duty_table;           // 源: 占空比查找表
    DMA_InitStruct.DstAddress = (uint32_t)(&CW_ATIM->CCR1);     // 目标: ATIM CCR1寄存器
    DMA_InitStruct.TriggerType = DMA_TRIGGER_HARDWARE;          // 硬件触发
    DMA_InitStruct.TriggerSource = DMA_TRIGGER_SRC_ATIM_UPD;    // 触发源: ATIM更新事件

    DMA_Init(DMA_CHANNEL_1, &DMA_InitStruct);

    /* 使能DMA CH1传输完成中断  */
    DMA_ITConfig(DMA_CHANNEL_1, DMA_INTERRUPT_TC, ENABLE);

    /* 使能DMA CH1 */
    DMA_Cmd(DMA_CHANNEL_1, ENABLE);
}

/**
  * @brief  NVIC中断配置
  *         使能DMA CH1/2 中断 (用于传输完成监控)
  * @param  None
  * @retval None
  */
static void NVIC_Configuration(void)
{
    __disable_irq();
    NVIC_EnableIRQ(DMACH12_IRQn);    // DMA CH1/2 中断
    __enable_irq();
}

/**
  * @brief  ATIM中断回调 (未使用, 保留)
  * @param  None
  * @retval None
  */
void ATIM_IRQHandlerCallBack(void)
{
    /* 不使用ATIM中断 */
}

/**
  * @brief  DMA CH1/2 中断回调
  *         DMA CH1传输完成时调用
  *         递增传输完成计数
  * @param  None
  * @retval None
  */
void DMACH12_IRQHandlerCallBack(void)
{
    /* 清除DMA CH1传输完成标志并计数 */
    /* 注意: CW32L012 DMA 的 TC 标志在 CSR1 bit12, ICR 清零对应位 */
    uint32_t csr1 = *DMA_CSRy(DMA_CHANNEL_1);
    if (csr1 & 0x1000)  // CSR1 bit12 = TC (Transfer Complete)
    {
        /* 清除DMA CH1传输完成中断标志 (写0到ICR对应位) */
        *DMA_ICR &= ~(1UL << (DMA_CHANNEL_1 * 4));  // TC1 at ICR bit 0

        /* 传输完成计数+1 */
        g_dma_tc_count++;
    }
}

/******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
