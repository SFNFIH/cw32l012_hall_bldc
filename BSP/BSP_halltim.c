/**
 * @file    BSP_halltim.c
 * @brief   HALLTIM 霍尔接口实现 (PB02/PB10/PB11)
 */
#include "BSP_halltim.h"

/* 最近一次采样的滤波状态 (bit0=CH1, bit1=CH2, bit2=CH3) */
volatile uint8_t g_hall_state = 0U;

static void BSP_HALLTIM_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    BSP_HALLTIM_GPIO_CLK_ENABLE();

    /* 霍尔网表经 100Ω 与 I2C(PB06/PB07) 耦合, 保持模拟以免干扰 */
    CW_GPIOB->ANALOG |= (GPIO_PIN_6 | GPIO_PIN_7);

    BSP_HALLTIM_CH1_AF();
    BSP_HALLTIM_CH2_AF();
    BSP_HALLTIM_CH3_AF();

    /* 数字输入 + 内部上拉; PB02 开漏霍尔建议再加外部上拉(如 4.7k~10k) */
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT_PULLUP;
    GPIO_InitStruct.IT   = GPIO_IT_NONE;
    GPIO_InitStruct.Pins = BSP_HALLTIM_CH1_PIN | BSP_HALLTIM_CH2_PIN | BSP_HALLTIM_CH3_PIN;
    GPIO_Init(CW_GPIOB, &GPIO_InitStruct);

    /* 再次确认退出模拟 (PB02 兼 ADC1_IN11) 并强制上拉 */
    PB02_DIGTAL_ENABLE();
    CW_GPIOB->ANALOG &= ~(BSP_HALLTIM_CH2_PIN | BSP_HALLTIM_CH3_PIN);
    PB02_PUR_ENABLE();
    PB10_PUR_ENABLE();
    PB11_PUR_ENABLE();
}

void BSP_HALLTIM_Init(void)
{
    HALLTIM_InitTypeDef hall_init;

    BSP_HALLTIM_GPIO_Init();

    /* FLT1 多数表决 + FLT2 约 1ms 毛刺滤波 (PCLK=8MHz) */
    hall_init.filter_enable     = TRUE;
    hall_init.filter_length     = 0U;
    hall_init.clock_division    = HALLTIM_DIV_PCLK;
    hall_init.trigger_output    = HALLTIM_MMS_CHANGE;
    hall_init.auto_reload_value = 0x00FFFFFFU;
    HALLTIM_Init(&hall_init);

    CW_HALLTIM->CR_f.FLT1EN  = 1U;
    CW_HALLTIM->CR_f.FLT2LEN = 8000U;

    CW_HALLTIM->DIER = 0U;
    HALLTIM_ClearStatus(HALLTIM_ISR_CHG | HALLTIM_ISR_OV | HALLTIM_ISR_MATCH);

    HALLTIM_Cmd(ENABLE);

    g_hall_state = BSP_HALLTIM_GetState();
}

uint8_t BSP_HALLTIM_GetState(void)
{
    uint8_t state = 0U;

    if (HALLTIM_GetFilteredStateCH1() != 0U)
    {
        state |= (1U << 0);
    }
    if (HALLTIM_GetFilteredStateCH2() != 0U)
    {
        state |= (1U << 1);
    }
    if (HALLTIM_GetFilteredStateCH3() != 0U)
    {
        state |= (1U << 2);
    }

    return state;
}

void BSP_HALLTIM_IRQHandler(void)
{
    uint32_t isr = HALLTIM_GetStatus();

    if ((isr & HALLTIM_ISR_CHG) != 0U)
    {
        g_hall_state = BSP_HALLTIM_GetState();
        HALLTIM_ClearStatus(HALLTIM_ISR_CHG);
    }

    if ((isr & HALLTIM_ISR_OV) != 0U)
    {
        HALLTIM_ClearStatus(HALLTIM_ISR_OV);
    }

    if ((isr & HALLTIM_ISR_MATCH) != 0U)
    {
        HALLTIM_ClearStatus(HALLTIM_ISR_MATCH);
    }
}
