/**
 * @file    BSP_halltim.c
 * @brief   HALLTIM 霍尔接口: 变化中断换相
 * @note    去掉“拒反向则不换相”(会导致错扇区堵转后彻底不换相);
 *          高速抖边改用换相后短空白忽略下一次边沿
 */
#include "BSP_halltim.h"
#include "BSP_motor.h"

volatile uint8_t g_hall_state = 0U;

/* #region agent log */
volatile uint16_t g_hall_skip_cnt = 0U;
volatile uint16_t g_hall_step_dt = 0U;
volatile uint16_t g_hall_fw_cnt = 0U;
volatile uint16_t g_hall_bw_cnt = 0U;
volatile uint16_t g_hall_flip_cnt = 0U;
volatile uint16_t g_hall_rej_cnt = 0U;    /* 换相空白期内忽略的边沿 */
volatile uint16_t g_hall_irq_cnt = 0U;
volatile uint16_t g_hall_ok_cnt = 0U;
volatile uint8_t  g_hall_prev = 0xFFU;
volatile int8_t   g_hall_dir = 0;
/* #endregion */

/* 正向: 001→011→010→110→100→101→001 */
static const uint8_t s_next_fw[8] = {
    0xFF, 0x03, 0x06, 0x02, 0x05, 0x01, 0x04, 0xFF
};
static const uint8_t s_next_bw[8] = {
    0xFF, 0x05, 0x03, 0x01, 0x06, 0x04, 0x02, 0xFF
};

/* 换相后忽略接下来 N 次 CHG, 抑制边界来回抖 */
static uint8_t s_blank_left = 0U;
static uint8_t s_auto_comm = 1U;

static void BSP_HALLTIM_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    BSP_HALLTIM_GPIO_CLK_ENABLE();

    CW_GPIOB->ANALOG |= (GPIO_PIN_6 | GPIO_PIN_7);

    BSP_HALLTIM_CH1_AF();
    BSP_HALLTIM_CH2_AF();
    BSP_HALLTIM_CH3_AF();

    GPIO_InitStruct.Mode = GPIO_MODE_INPUT_PULLUP;
    GPIO_InitStruct.IT   = GPIO_IT_NONE;
    GPIO_InitStruct.Pins = BSP_HALLTIM_CH1_PIN | BSP_HALLTIM_CH2_PIN | BSP_HALLTIM_CH3_PIN;
    GPIO_Init(CW_GPIOB, &GPIO_InitStruct);

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

    /* FLT2 ~250us @ 8MHz */
    hall_init.filter_enable     = TRUE;
    hall_init.filter_length     = 0U;
    hall_init.clock_division    = HALLTIM_DIV_PCLK;
    hall_init.trigger_output    = HALLTIM_MMS_CHANGE;
    hall_init.auto_reload_value = 0x00FFFFFFU;
    HALLTIM_Init(&hall_init);

    CW_HALLTIM->CR_f.FLT1EN  = 1U;
    CW_HALLTIM->CR_f.FLT2LEN = 2000U;

    CW_HALLTIM->DIER = 0U;
    HALLTIM_ClearStatus(HALLTIM_ISR_CHG | HALLTIM_ISR_OV | HALLTIM_ISR_MATCH);

    CW_HALLTIM->DIER_f.CAPIE = 1U;
    NVIC_ClearPendingIRQ(BSP_HALLTIM_IRQn);
    NVIC_SetPriority(BSP_HALLTIM_IRQn, 0U);
    NVIC_EnableIRQ(BSP_HALLTIM_IRQn);

    HALLTIM_Cmd(ENABLE);

    g_hall_state = BSP_HALLTIM_GetState();
    g_hall_prev = g_hall_state;
    g_hall_dir = 0;
    s_blank_left = 0U;
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

/**
 * @brief  处理一次霍尔跳变并可选换相
 * @return 1=已换相, 0=忽略/无效
 */
uint8_t BSP_HALLTIM_ApplyHall(uint8_t hall, uint8_t do_commutate)
{
    uint8_t prev = g_hall_prev;
    int8_t step_dir = 0;

    hall &= 0x07U;
    if ((hall == 0U) || (hall == 7U) || (hall == prev))
    {
        return 0U;
    }

    /* #region agent log */
    g_hall_step_dt = (uint16_t)(HALLTIM_GetPulseWidth() & 0xFFFFU);
    /* #endregion */

    /* 换相后短空白: 只更新状态, 不换相 (抑制抖边换向) */
    if ((do_commutate != 0U) && (s_blank_left > 0U))
    {
        s_blank_left--;
        g_hall_state = hall;
        /* #region agent log */
        if (g_hall_rej_cnt < 0xFFFFU)
        {
            g_hall_rej_cnt++;
        }
        /* #endregion */
        return 0U;
    }

    if ((prev != 0xFFU) && (prev != 0U) && (prev != 7U))
    {
        if (s_next_fw[prev] == hall)
        {
            step_dir = 1;
        }
        else if (s_next_bw[prev] == hall)
        {
            step_dir = -1;
        }
        else
        {
            /* #region agent log */
            if (g_hall_skip_cnt < 0xFFFFU)
            {
                g_hall_skip_cnt++;
            }
            /* #endregion */
        }
    }

    if (step_dir != 0)
    {
        if (g_hall_dir == 0)
        {
            g_hall_dir = step_dir;
        }
        else if (step_dir != g_hall_dir)
        {
            g_hall_dir = step_dir;
            /* #region agent log */
            if (g_hall_flip_cnt < 0xFFFFU)
            {
                g_hall_flip_cnt++;
            }
            /* #endregion */
        }

        /* #region agent log */
        if (step_dir > 0)
        {
            if (g_hall_fw_cnt < 0xFFFFU) { g_hall_fw_cnt++; }
        }
        else
        {
            if (g_hall_bw_cnt < 0xFFFFU) { g_hall_bw_cnt++; }
        }
        /* #endregion */
    }

    g_hall_prev = hall;
    g_hall_state = hall;
    if (do_commutate != 0U)
    {
        BSP_MOTOR_Commutate(hall);
        s_blank_left = 1U; /* 忽略紧随其后的 1 次抖边 */
        /* #region agent log */
        if (g_hall_ok_cnt < 0xFFFFU)
        {
            g_hall_ok_cnt++;
        }
        /* #endregion */
    }
    return 1U;
}

void BSP_HALLTIM_IRQHandler(void)
{
    uint32_t isr = HALLTIM_GetStatus();

    if ((isr & HALLTIM_ISR_CHG) != 0U)
    {
        uint8_t hall = BSP_HALLTIM_GetState();
        /* #region agent log */
        if (g_hall_irq_cnt < 0xFFFFU)
        {
            g_hall_irq_cnt++;
        }
        /* #endregion */
        if ((s_auto_comm != 0U) && (BSP_MOTOR_IsRunning() != 0U))
        {
            (void)BSP_HALLTIM_ApplyHall(hall, 1U);
        }
        else
        {
            g_hall_state = hall;
            g_hall_prev = hall;
        }
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

void BSP_HALLTIM_NoteTransition(uint8_t hall)
{
    (void)BSP_HALLTIM_ApplyHall(hall, 0U);
}

void BSP_HALLTIM_ResetDirection(void)
{
    uint8_t hall = BSP_HALLTIM_GetState();
    g_hall_state = hall;
    g_hall_prev = hall;
    g_hall_dir = 0;
    s_blank_left = 0U;
    g_hall_fw_cnt = 0U;
    g_hall_bw_cnt = 0U;
    g_hall_flip_cnt = 0U;
    g_hall_rej_cnt = 0U;
    g_hall_skip_cnt = 0U;
    g_hall_irq_cnt = 0U;
    g_hall_ok_cnt = 0U;
}

void BSP_HALLTIM_SetAutoCommutate(uint8_t enable)
{
    s_auto_comm = (enable != 0U) ? 1U : 0U;
}
