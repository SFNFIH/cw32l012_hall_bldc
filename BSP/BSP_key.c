/**
 * @file    BSP_key.c
 * @brief   PC13 按键 + BTIM1 10ms 扫描
 */
#include "BSP_key.h"

#define KEY_DEBOUNCE_CNT    2U   /* 连续 2 次相同才确认, 约 20ms */

static volatile uint8_t    s_stable   = 0U;  /* 消抖后电平, 1=按下 */
static volatile uint8_t    s_raw_last = 0U;
static volatile uint8_t    s_same_cnt = 0U;
static volatile KeyEvent_t s_event    = KEY_EVT_NONE;

/* CW32L012: PDR 在 OPENDRAIN 与 PUR 之间 (offset 0x08), 头文件标为 RESERVED */
#define GPIOx_PDR(GPIOx)  (*(volatile uint32_t *)((uint32_t)(GPIOx) + 0x08U))

static void KEY_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    BSP_KEY1_CLK_ENABLE();
    BSP_KEY1_AF();

    /* 空闲低、按下高 → 输入 + 内部下拉 */
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.IT   = GPIO_IT_NONE;
    GPIO_InitStruct.Pins = BSP_KEY1_PIN;
    GPIO_Init(BSP_KEY1_PORT, &GPIO_InitStruct);

    PC13_PUR_DISABLE();
    GPIOx_PDR(BSP_KEY1_PORT) |= BSP_KEY1_PIN;
}

static void KEY_Timer_Init(uint32_t pclk_hz)
{
    BTIM_TimeBaseInitTypeDef tb;
    uint32_t ticks_10ms;

    /* 10ms 中断: cnt_clk = PCLK/8000 = 1kHz @ 8MHz, ARR=9 → 10ms
     * PSC 直接写入分频系数 (WHXY BTIM: 计数时钟 = PCLK / PSC) */
    (void)pclk_hz;
    ticks_10ms = 10U;

    __SYSCTRL_BTIM123_CLK_ENABLE();

    tb.BTIM_Prescaler = 8000U;
    tb.BTIM_Period    = (uint16_t)(ticks_10ms - 1U);
    tb.BTIM_Mode      = BTIM_MODE_TIMER;
    tb.BTIM_CountMode = BTIM_COUNT_MODE_REPETITIVE;
    BTIM_TimeBaseInit(BSP_KEY_BTIM, &tb);

    BTIM_ClearITPendingBit(BSP_KEY_BTIM, BTIM_IT_UPDATE);
    BTIM_ITConfig(BSP_KEY_BTIM, BTIM_IT_UPDATE, ENABLE);

    NVIC_ClearPendingIRQ(BSP_KEY_BTIM_IRQn);
    NVIC_SetPriority(BSP_KEY_BTIM_IRQn, 2U);
    NVIC_EnableIRQ(BSP_KEY_BTIM_IRQn);

    BTIM_Cmd(BSP_KEY_BTIM, ENABLE);
}

void BSP_KEY_Init(uint32_t pclk_hz)
{
    s_stable   = 0U;
    s_raw_last = 0U;
    s_same_cnt = 0U;
    s_event    = KEY_EVT_NONE;

    KEY_GPIO_Init();
    KEY_Timer_Init(pclk_hz);
}

void BSP_KEY_Scan(void)
{
    uint8_t raw = (BSP_KEY1_READ() != 0U) ? 1U : 0U;

    if (raw == s_raw_last)
    {
        if (s_same_cnt < 0xFFU)
        {
            s_same_cnt++;
        }
    }
    else
    {
        s_raw_last = raw;
        s_same_cnt = 1U;
    }

    if ((s_same_cnt >= KEY_DEBOUNCE_CNT) && (raw != s_stable))
    {
        s_stable = raw;
        if (raw != 0U)
        {
            s_event = KEY_EVT_PRESS;
        }
        else
        {
            s_event = KEY_EVT_RELEASE;
        }
    }
}

KeyEvent_t BSP_KEY_GetEvent(void)
{
    KeyEvent_t ev = s_event;
    s_event = KEY_EVT_NONE;
    return ev;
}

uint8_t BSP_KEY_IsPressed(void)
{
    return s_stable;
}

void BSP_KEY_TimerIRQHandler(void)
{
    if (BTIM_GetITStatus(BSP_KEY_BTIM, BTIM_IT_UPDATE) != RESET)
    {
        BTIM_ClearITPendingBit(BSP_KEY_BTIM, BTIM_IT_UPDATE);
        BSP_KEY_Scan();
    }
}
