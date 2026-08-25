/**
 * @file    BSP_motor.c
 * @brief   三相互补中心对齐 PWM, 供 FOC/SVPWM 使用
 */
#include "BSP_motor.h"

static uint8_t s_running = 0U;

static void MOTOR_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    __SYSCTRL_GPIOA_CLK_ENABLE();
    __SYSCTRL_GPIOB_CLK_ENABLE();

    PA08_AFx_ATIMCH1();
    PA09_AFx_ATIMCH2();
    PA10_AFx_ATIMCH3();
    PB13_AFx_ATIMCH1N();
    PB14_AFx_ATIMCH2N();
    PB15_AFx_ATIMCH3N();

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.IT   = GPIO_IT_NONE;
    GPIO_InitStruct.Pins = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    GPIO_Init(CW_GPIOA, &GPIO_InitStruct);
    GPIO_InitStruct.Pins = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_Init(CW_GPIOB, &GPIO_InitStruct);
}

static uint16_t ClampCcr(uint16_t d)
{
    if (d < 2U)
    {
        d = 2U;
    }
    if (d > (BSP_MOTOR_PWM_ARR - 2U))
    {
        d = (uint16_t)(BSP_MOTOR_PWM_ARR - 2U);
    }
    return d;
}

void BSP_MOTOR_Init(uint32_t pclk_hz)
{
    ATIM_InitTypeDef   tim;
    ATIM_OCInitTypeDef oc;
    (void)pclk_hz;

    MOTOR_GPIO_Init();

    __SYSCTRL_ATIM_CLK_ENABLE();
    ATIM_DeInit();

    tim.BufferState        = ENABLE;
    tim.CounterAlignedMode = ATIM_COUNT_ALIGN_MODE_CENTER_BOTH;
    tim.CounterDirection   = ATIM_COUNTING_UP;
    tim.CounterOPMode      = ATIM_OP_MODE_REPETITIVE;
    tim.Prescaler          = 0U;
    tim.ReloadValue        = BSP_MOTOR_PWM_ARR;
    /* RCR=0: 峰/谷都出 UIF, FOC 只在峰值采样三电阻 */
    tim.RepetitionCounter  = 0U;
    ATIM_Init(&tim);

    oc.OCPolarity       = ATIM_OCPOLARITY_NONINVERT;
    oc.OCMode           = ATIM_OCMODE_PWM1;
    oc.OCFastMode       = ATIM_OC_FAST_MODE_DISABLE;
    oc.OCInterruptState = DISABLE;
    oc.BufferState      = ENABLE;
    oc.OCComplement     = ENABLE;

    ATIM_OC1Init(&oc);
    ATIM_OC2Init(&oc);
    ATIM_OC3Init(&oc);

    ATIM_SetCompare1(BSP_MOTOR_DUTY_MID);
    ATIM_SetCompare2(BSP_MOTOR_DUTY_MID);
    ATIM_SetCompare3(BSP_MOTOR_DUTY_MID);

    /* 三相一直互补 PWM */
    CW_ATIM->CCER = 0x555UL;

    ATIM_SetPWMDeadtime((int16_t)BSP_MOTOR_PWM_DEADTIME,
                        (int16_t)BSP_MOTOR_PWM_DEADTIME,
                        DISABLE);

    CW_ATIM->BDTR_f.OSSR = 1U;
    CW_ATIM->BDTR_f.OSSI = 0U;
    CW_ATIM->BDTR_f.BKE  = 0U;
    CW_ATIM->BDTR_f.BK2E = 0U;
    CW_ATIM->AF1_f.BKINE = 0U;

    ATIM_Cmd(ENABLE);
    ATIM_CtrlPWMOutputs(DISABLE);
    s_running = 0U;
}

void BSP_MOTOR_SetPhaseDuty(uint16_t du, uint16_t dv, uint16_t dw)
{
    ATIM_SetCompare3(ClampCcr(du)); /* U → CH3 PA10 */
    ATIM_SetCompare2(ClampCcr(dv)); /* V → CH2 PA09 */
    ATIM_SetCompare1(ClampCcr(dw)); /* W → CH1 PA08 */
}

void BSP_MOTOR_Start(void)
{
    BSP_MOTOR_SetPhaseDuty(BSP_MOTOR_DUTY_MID, BSP_MOTOR_DUTY_MID, BSP_MOTOR_DUTY_MID);
    CW_ATIM->CCER = 0x555UL;
    ATIM_CtrlPWMOutputs(ENABLE);
    s_running = 1U;
}

void BSP_MOTOR_Stop(void)
{
    ATIM_CtrlPWMOutputs(DISABLE);
    CW_ATIM->CCER = 0U;
    s_running = 0U;
}

uint8_t BSP_MOTOR_IsRunning(void)
{
    return s_running;
}

uint8_t BSP_MOTOR_IsPeakUpdate(void)
{
    /* DIR=1 正在下计, 说明刚过 ARR 峰值, 下桥导通窗 */
    return (CW_ATIM->CR1_f.DIR != 0U) ? 1U : 0U;
}

void BSP_MOTOR_EnablePwmIrq(void)
{
    ATIM_ClearITPendingBit(ATIM_STATE_UIF);
    ATIM_ITConfig(ATIM_IT_UIE, ENABLE);
    NVIC_ClearPendingIRQ(ATIM_IRQn);
    NVIC_SetPriority(ATIM_IRQn, 0U);
    NVIC_EnableIRQ(ATIM_IRQn);
}
