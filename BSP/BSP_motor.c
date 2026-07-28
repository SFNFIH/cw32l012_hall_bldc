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

static void MOTOR_SetPhase(uint8_t ch, uint8_t mode)
{
    switch (mode)
    {
        case PHASE_PWM:
            /* 上桥 PWM + 下桥互补 PWM (死区) */
            MOTOR_SetOcMode(ch, ATIM_OCMODE_PWM1);
            if (ch == 0U)
            {
                CW_ATIM->CCER_f.CC1E  = 1U;
                CW_ATIM->CCER_f.CC1NE = 1U;
            }
            else if (ch == 1U)
            {
                CW_ATIM->CCER_f.CC2E  = 1U;
                CW_ATIM->CCER_f.CC2NE = 1U;
            }
            else
            {
                CW_ATIM->CCER_f.CC3E  = 1U;
                CW_ATIM->CCER_f.CC3NE = 1U;
            }
            break;

        case PHASE_LOW:
            /* OCxREF=0 → 互补下桥为高, 仅开 CHN */
            MOTOR_SetOcMode(ch, ATIM_OCMODE_FORCED_INACTIVE);
            if (ch == 0U)
            {
                CW_ATIM->CCER_f.CC1E  = 0U;
                CW_ATIM->CCER_f.CC1NE = 1U;
            }
            else if (ch == 1U)
            {
                CW_ATIM->CCER_f.CC2E  = 0U;
                CW_ATIM->CCER_f.CC2NE = 1U;
            }
            else
            {
                CW_ATIM->CCER_f.CC3E  = 0U;
                CW_ATIM->CCER_f.CC3NE = 1U;
            }
            break;

        default: /* PHASE_FLOAT */
            if (ch == 0U)
            {
                CW_ATIM->CCER_f.CC1E  = 0U;
                CW_ATIM->CCER_f.CC1NE = 0U;
            }
            else if (ch == 1U)
            {
                CW_ATIM->CCER_f.CC2E  = 0U;
                CW_ATIM->CCER_f.CC2NE = 0U;
            }
            else
            {
                CW_ATIM->CCER_f.CC3E  = 0U;
                CW_ATIM->CCER_f.CC3NE = 0U;
            }
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
    tim.RepetitionCounter = 0U;
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

    /* 运行/空闲时关闭态为无效电平 */
    CW_ATIM->BDTR_f.OSSR = 1U;
    CW_ATIM->BDTR_f.OSSI = 1U;

    ATIM_Cmd(ENABLE);
    ATIM_CtrlPWMOutputs(DISABLE); /* Start 时再开 MOE */

    s_running = 0U;
}

void BSP_MOTOR_SetDuty(uint16_t duty)
{
    if (duty > BSP_MOTOR_PWM_ARR)
    {
        duty = BSP_MOTOR_PWM_ARR;
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

    hall &= 0x07U;
    if ((hall == 0U) || (hall == 7U))
    {
        MOTOR_AllFloat();
        return;
    }

    pwm_ch = s_step_table[hall][0];
    low_ch = s_step_table[hall][1];

    /* 先全部浮空再开通, 避免换相直通 */
    MOTOR_AllFloat();

    for (i = 0U; i < 3U; i++)
    {
        if (i == pwm_ch)
        {
            MOTOR_SetPhase(i, PHASE_PWM);
        }
        else if (i == low_ch)
        {
            MOTOR_SetPhase(i, PHASE_LOW);
        }
        else
        {
            MOTOR_SetPhase(i, PHASE_FLOAT);
        }
    }
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
