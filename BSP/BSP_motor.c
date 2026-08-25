/**
 * @file    BSP_motor.c
 * @brief   BLDC 六步换相实现
 */
#include "BSP_motor.h"

/* 相状态 */
#define PHASE_FLOAT   0U   /* CCxE=0, CCxNE=0 */
#define PHASE_PWM     1U   /* 上下桥互补 PWM */
#define PHASE_LOW     2U   /* 仅下桥常开 */

static uint16_t s_duty = BSP_MOTOR_DUTY_DEFAULT;
static uint8_t  s_running = 0U;

/* hall(1..6) → {pwm_phase, low_phase}, phase: 0=A/CH1, 1=B/CH2, 2=C/CH3
 * 对应扇区序列 001→011→010→110→100→101 */
static const uint8_t s_step_table[8][2] = {
    /* 000 invalid */ {0, 0},
    /* 001 */ {0, 1},  /* A+ B- */
    /* 010 */ {2, 0},  /* C+ A- */
    /* 011 */ {2, 1},  /* C+ B- */
    /* 100 */ {1, 2},  /* B+ C- */
    /* 101 */ {0, 2},  /* A+ C- */
    /* 110 */ {1, 0},  /* B+ A- */
    /* 111 invalid */ {0, 0},
};

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

static void MOTOR_SetOcMode(uint8_t ch, uint32_t oc_mode)
{
    uint32_t m  = oc_mode & 0x07U;
    uint32_t mh = oc_mode >> 3;

    switch (ch)
    {
        case 0:
            CW_ATIM->CCMR1CMP_f.OC1M  = m;
            CW_ATIM->CCMR1CMP_f.OC1MH = mh;
            break;
        case 1:
            CW_ATIM->CCMR1CMP_f.OC2M  = m;
            CW_ATIM->CCMR1CMP_f.OC2MH = mh;
            break;
        default:
            CW_ATIM->CCMR2CMP_f.OC3M  = m;
            CW_ATIM->CCMR2CMP_f.OC3MH = mh;
            break;
    }
}

/* 原子写单相 CCxE/CCxNE, 避免位域 RMW 中间态 */
static void MOTOR_SetCcer(uint8_t ch, uint8_t en, uint8_t nen)
{
    uint32_t ccer = CW_ATIM->CCER;
    uint32_t shift = (uint32_t)ch * 4U;
    ccer &= ~((1UL << shift) | (1UL << (shift + 2U)));
    if (en != 0U)
    {
        ccer |= (1UL << shift);
    }
    if (nen != 0U)
    {
        ccer |= (1UL << (shift + 2U));
    }
    CW_ATIM->CCER = ccer;
}

static void MOTOR_SetPhase(uint8_t ch, uint8_t mode)
{
    switch (mode)
    {
        case PHASE_PWM:
            MOTOR_SetOcMode(ch, ATIM_OCMODE_PWM1);
            MOTOR_SetCcer(ch, 1U, 1U);
            break;

        case PHASE_LOW:
            /* SWD 实测: 仅 CCxNE=1 时 PB13/14/15 仍为低, 下桥不开
             * 必须 CCxE+CCxNE 同时开 + FORCED_INACTIVE → CHN 才为高 */
            MOTOR_SetOcMode(ch, ATIM_OCMODE_FORCED_INACTIVE);
            MOTOR_SetCcer(ch, 1U, 1U);
            break;

        default:
            MOTOR_SetCcer(ch, 0U, 0U);
            break;
    }
}

static void MOTOR_AllFloat(void)
{
    MOTOR_SetPhase(0, PHASE_FLOAT);
    MOTOR_SetPhase(1, PHASE_FLOAT);
    MOTOR_SetPhase(2, PHASE_FLOAT);
}

void BSP_MOTOR_Init(uint32_t pclk_hz)
{
    ATIM_InitTypeDef   tim;
    ATIM_OCInitTypeDef oc;
    (void)pclk_hz;

    MOTOR_GPIO_Init();

    __SYSCTRL_ATIM_CLK_ENABLE();
    ATIM_DeInit();

    tim.BufferState       = ENABLE;
    tim.CounterAlignedMode = ATIM_COUNT_ALIGN_MODE_CENTER_BOTH;
    tim.CounterDirection  = ATIM_COUNTING_UP;
    tim.CounterOPMode     = ATIM_OP_MODE_REPETITIVE;
    tim.Prescaler         = 0U;
    tim.ReloadValue       = BSP_MOTOR_PWM_ARR;
    /* RCR=1: 中央对齐下溢+溢出各计一次, UIF 每 PWM 周期一次 */
    tim.RepetitionCounter = 1U;
    ATIM_Init(&tim);

    oc.OCPolarity        = ATIM_OCPOLARITY_NONINVERT;
    oc.OCMode            = ATIM_OCMODE_PWM1;
    oc.OCFastMode        = ATIM_OC_FAST_MODE_DISABLE;
    oc.OCInterruptState  = DISABLE;
    oc.BufferState       = ENABLE;
    oc.OCComplement      = ENABLE; /* 使能 CHxN */

    ATIM_OC1Init(&oc);
    ATIM_OC2Init(&oc);
    ATIM_OC3Init(&oc);

    /* 初始关闭主通道, 换相时再开 */
    ATIM_CH1Config(DISABLE);
    ATIM_CH2Config(DISABLE);
    ATIM_CH3Config(DISABLE);
    CW_ATIM->CCER_f.CC1NE = 0U;
    CW_ATIM->CCER_f.CC2NE = 0U;
    CW_ATIM->CCER_f.CC3NE = 0U;

    ATIM_SetCompare1(s_duty);
    ATIM_SetCompare2(s_duty);
    ATIM_SetCompare3(s_duty);

    ATIM_SetPWMDeadtime((int16_t)BSP_MOTOR_PWM_DEADTIME,
                        (int16_t)BSP_MOTOR_PWM_DEADTIME,
                        DISABLE);

    /* OSSR=1 运行关断无效; OSSI=0 禁用通道真正 Hi-Z (浮空相)
     * 明确关闭刹车, 避免未接 BKIN 误关断 */
    CW_ATIM->BDTR_f.OSSR = 1U;
    CW_ATIM->BDTR_f.OSSI = 0U;
    CW_ATIM->BDTR_f.BKE  = 0U;
    CW_ATIM->BDTR_f.BK2E = 0U;
    CW_ATIM->AF1_f.BKINE = 0U;

    ATIM_Cmd(ENABLE);
    ATIM_CtrlPWMOutputs(DISABLE); /* Start 时再开 MOE */

    s_running = 0U;
}

void BSP_MOTOR_SetDuty(uint16_t duty)
{
    /* CCR==ARR 时中央对齐 PWM1 近似常通, 看起来像“全开且不换相” */
    if (duty >= BSP_MOTOR_PWM_ARR)
    {
        duty = (uint16_t)(BSP_MOTOR_PWM_ARR - 1U);
    }
    s_duty = duty;
    ATIM_SetCompare1(duty);
    ATIM_SetCompare2(duty);
    ATIM_SetCompare3(duty);
}

uint16_t BSP_MOTOR_GetDuty(void)
{
    return s_duty;
}

void BSP_MOTOR_Commutate(uint8_t hall)
{
    uint8_t pwm_ch;
    uint8_t low_ch;
    uint8_t i;
    uint32_t ccer;

    hall &= 0x07U;
    if ((hall == 0U) || (hall == 7U))
    {
        MOTOR_AllFloat();
        return;
    }

    pwm_ch = s_step_table[hall][0];
    low_ch = s_step_table[hall][1];

    /* 先改 OC 模式, 再一次写 CCER (无浮空空白, 高速少失步) */
    for (i = 0U; i < 3U; i++)
    {
        if (i == pwm_ch)
        {
            MOTOR_SetOcMode(i, ATIM_OCMODE_PWM1);
        }
        else if (i == low_ch)
        {
            MOTOR_SetOcMode(i, ATIM_OCMODE_FORCED_INACTIVE);
        }
    }

    ccer = CW_ATIM->CCER;
    ccer &= ~0x777UL;
    for (i = 0U; i < 3U; i++)
    {
        uint32_t shift = (uint32_t)i * 4U;
        if ((i == pwm_ch) || (i == low_ch))
        {
            /* PWM 与 LOW 均 E+NE (LOW 用 forced inactive 时 CHN 才拉高) */
            ccer |= (1UL << shift) | (1UL << (shift + 2U));
        }
    }
    CW_ATIM->CCER = ccer;
}

void BSP_MOTOR_Start(uint8_t hall)
{
    BSP_MOTOR_SetDuty(s_duty);
    BSP_MOTOR_Commutate(hall);
    ATIM_CtrlPWMOutputs(ENABLE);
    s_running = 1U;
}

void BSP_MOTOR_Stop(void)
{
    MOTOR_AllFloat();
    ATIM_CtrlPWMOutputs(DISABLE);
    s_running = 0U;
}

uint8_t BSP_MOTOR_IsRunning(void)
{
    return s_running;
}

/* 正向: 001 → 011 → 010 → 110 → 100 → 101 */
static const uint8_t s_step_to_hall[6] = {
    0x01U, 0x03U, 0x02U, 0x06U, 0x04U, 0x05U
};

uint8_t BSP_MOTOR_StepToHall(uint8_t step)
{
    return s_step_to_hall[step % 6U];
}

void BSP_MOTOR_CommutateStep(uint8_t step)
{
    BSP_MOTOR_Commutate(BSP_MOTOR_StepToHall(step));
}

void BSP_MOTOR_EnablePwmIrq(void)
{
    ATIM_ClearITPendingBit(ATIM_STATE_UIF);
    ATIM_ITConfig(ATIM_IT_UIE, ENABLE);
    NVIC_ClearPendingIRQ(ATIM_IRQn);
    NVIC_SetPriority(ATIM_IRQn, 0U);
    NVIC_EnableIRQ(ATIM_IRQn);
}
