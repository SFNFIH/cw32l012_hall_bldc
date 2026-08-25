/**
 * @file    main.c
 * @brief   无感 FOC (d 轴方波高频注入) + 电位器 Iq + 按键启停
 *
 * 串口默认 VOFA+ JustFloat (115200). 改 APP_VOFA_ENABLE=0 恢复 ASCII DBG.
 */
#include "../inc/main.h"

#define APP_HCLK_HZ         8000000U
#define APP_PCLK_HZ         APP_HCLK_HZ
#define APP_UART_BAUD       115200U
#define IQ_MIN              80U
#define IQ_MAX              900U
#define APP_VOFA_ENABLE     1U
#define APP_VOFA_PERIOD_MS  10U
#define VOFA_CH_COUNT       8U

#define DBG_MBOX_MAGIC  0x49B1DB60UL
typedef struct
{
    uint32_t magic;
    uint8_t  motor_on;
    uint8_t  state;
    uint16_t iq_cmd;
    uint16_t adc;
    int16_t  id;
    int16_t  iq;
    int16_t  demod;
    int16_t  omega;
    uint16_t theta;
    uint8_t  hall;
    uint8_t  moe;
    uint32_t stamp;
} DebugMbox_t;

volatile DebugMbox_t g_dbg_mbox;
static uint32_t s_dbg_stamp = 0U;

#if (APP_VOFA_ENABLE == 0U)
static const char *FocStateName(FocState_t st)
{
    switch (st)
    {
        case FOC_ST_ALIGN: return "ALIGN";
        case FOC_ST_IF:    return "IF";
        case FOC_ST_RUN:   return "RUN";
        case FOC_ST_FAULT: return "FAULT";
        default:           return "IDLE";
    }
}
#endif

static void DBG_UpdateMbox(uint8_t motor_on, uint16_t iq_cmd, uint16_t adc)
{
    FocState_t st = BSP_FOC_GetState();

    s_dbg_stamp++;
    g_dbg_mbox.magic    = DBG_MBOX_MAGIC;
    g_dbg_mbox.motor_on = motor_on;
    g_dbg_mbox.state    = (uint8_t)st;
    g_dbg_mbox.iq_cmd   = iq_cmd;
    g_dbg_mbox.adc      = adc;
    g_dbg_mbox.id       = BSP_FOC_GetId();
    g_dbg_mbox.iq       = BSP_FOC_GetIq();
    g_dbg_mbox.demod    = BSP_FOC_GetDemod();
    g_dbg_mbox.omega    = BSP_FOC_GetOmega();
    g_dbg_mbox.theta    = BSP_FOC_GetTheta();
    g_dbg_mbox.hall     = g_hall_state;
    g_dbg_mbox.moe      = (uint8_t)CW_ATIM->BDTR_f.MOE;
    g_dbg_mbox.stamp    = s_dbg_stamp;

#if (APP_VOFA_ENABLE == 0U)
    printf("DBG st=%s th=%u hall=%u id=%d iq=%d iqref=%d dem=%d w=%d\r\n",
           FocStateName(st),
           (unsigned int)(g_dbg_mbox.theta >> 8),
           (unsigned int)g_dbg_mbox.hall,
           (int)g_dbg_mbox.id,
           (int)g_dbg_mbox.iq,
           (int)iq_cmd,
           (int)g_dbg_mbox.demod,
           (int)g_dbg_mbox.omega);
#else
    (void)st;
#endif
}

#if (APP_VOFA_ENABLE != 0U)
/*
 * VOFA+ JustFloat 通道:
 *   0 id   1 iq   2 iqref   3 theta_deg
 *   4 dem  5 w    6 state   7 hall
 */
static void VofaSend(uint16_t iq_cmd)
{
    float data[VOFA_CH_COUNT];

    data[0] = (float)BSP_FOC_GetId();
    data[1] = (float)BSP_FOC_GetIq();
    data[2] = (float)iq_cmd;
    data[3] = ((float)BSP_FOC_GetTheta()) * (360.0f / 65536.0f);
    data[4] = (float)BSP_FOC_GetDemod();
    data[5] = (float)BSP_FOC_GetOmega();
    data[6] = (float)BSP_FOC_GetState();
    data[7] = (float)g_hall_state;
    BSP_USART_SendJustFloat(data, VOFA_CH_COUNT);
}
#endif

static void SYSCTRL_Configuration(void);
static uint16_t AdcToIq(uint16_t adc);
static void Motor_SetEnable(uint8_t on, uint16_t iq_cmd, uint16_t adc);

void InitTick(uint32_t HclkFreq)
{
    SysTick->LOAD = (HclkFreq / 1000U) - 1U;
    SysTick->VAL  = 0U;
    NVIC_SetPriority(SysTick_IRQn, 3U);
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;
}

static uint16_t AdcToIq(uint16_t adc)
{
    uint32_t iq;

    if (adc > BSP_ADC_RESOLUTION)
    {
        adc = (uint16_t)BSP_ADC_RESOLUTION;
    }
    iq = ((uint32_t)adc * IQ_MAX) / BSP_ADC_RESOLUTION;
    if (iq > IQ_MAX)
    {
        iq = IQ_MAX;
    }
    if (iq < IQ_MIN)
    {
        iq = IQ_MIN;
    }
    return (uint16_t)iq;
}

static void Motor_SetEnable(uint8_t on, uint16_t iq_cmd, uint16_t adc)
{
    if (on != 0U)
    {
        BSP_FOC_Start(iq_cmd);
        BSP_LED_On();
#if (APP_VOFA_ENABLE == 0U)
        printf("MOTOR ON iq=%u (FOC + HFI)\r\n", (unsigned int)iq_cmd);
#endif
    }
    else
    {
        BSP_FOC_Stop();
        BSP_LED_Off();
#if (APP_VOFA_ENABLE == 0U)
        printf("MOTOR OFF st=%s th=%u dem=%d w=%d\r\n",
               FocStateName(BSP_FOC_GetState()),
               (unsigned int)(BSP_FOC_GetTheta() >> 8),
               (int)BSP_FOC_GetDemod(),
               (int)BSP_FOC_GetOmega());
#endif
    }
    DBG_UpdateMbox(on, iq_cmd, adc);
}

int main(void)
{
    uint8_t motor_on = 0U;
    uint16_t adc;
    uint16_t iq_cmd;
    uint16_t iq_last = 0xFFFFU;
    KeyEvent_t key_ev;
#if (APP_VOFA_ENABLE == 0U)
    static uint32_t s_dbg_div = 0U;
    static uint8_t s_st_last = 0xFFU;
#else
    uint32_t vofa_ms = 0U;
#endif

    SYSCTRL_Configuration();
    InitTick(APP_HCLK_HZ);

    BSP_LED_Init();
    BSP_USART_Init(APP_PCLK_HZ, APP_UART_BAUD);
    BSP_HALLTIM_Init();
    BSP_HALLTIM_SetAutoCommutate(0U);
    BSP_KEY_Init(APP_PCLK_HZ);
    BSP_ADC_Init();
    BSP_MOTOR_Init(APP_PCLK_HZ);
    BSP_MOTOR_EnablePwmIrq();
    BSP_FOC_Init();

    NVIC_DisableIRQ(ATIM_IRQn);
    BSP_ADC_Convert(&adc);
    NVIC_EnableIRQ(ATIM_IRQn);

    iq_cmd = AdcToIq(adc);
    iq_last = iq_cmd;
    Motor_SetEnable(0U, iq_cmd, adc);

#if (APP_VOFA_ENABLE == 0U)
    printf("FOC+HFI | KEY=toggle | Iq %u..%u (ADC counts)\r\n",
           (unsigned int)IQ_MIN, (unsigned int)IQ_MAX);
#else
    vofa_ms = GetTick();
#endif

    while (1)
    {
        NVIC_DisableIRQ(ATIM_IRQn);
        BSP_ADC_Convert(&adc);
        NVIC_EnableIRQ(ATIM_IRQn);

        iq_cmd = AdcToIq(adc);

        key_ev = BSP_KEY_GetEvent();
        if (key_ev == KEY_EVT_PRESS)
        {
            motor_on = (motor_on != 0U) ? 0U : 1U;
            Motor_SetEnable(motor_on, iq_cmd, adc);
            iq_last = iq_cmd;
        }

        if (BSP_FOC_GetState() == FOC_ST_FAULT)
        {
            if (motor_on != 0U)
            {
                motor_on = 0U;
                BSP_LED_Off();
#if (APP_VOFA_ENABLE == 0U)
                printf("MOTOR FAULT overcurrent — press KEY to retry\r\n");
#endif
            }
        }

        if (motor_on != 0U)
        {
            if (iq_cmd != iq_last)
            {
                iq_last = iq_cmd;
                BSP_FOC_SetIqRef(iq_cmd);
            }

#if (APP_VOFA_ENABLE == 0U)
            s_dbg_div++;
            if ((BSP_FOC_GetState() != (FocState_t)s_st_last) || (s_dbg_div >= 4000U))
            {
                s_st_last = (uint8_t)BSP_FOC_GetState();
                s_dbg_div = 0U;
                DBG_UpdateMbox(motor_on, iq_cmd, adc);
            }
#endif
        }
        else
        {
            iq_last = iq_cmd;
        }

#if (APP_VOFA_ENABLE != 0U)
        if ((GetTick() - vofa_ms) >= APP_VOFA_PERIOD_MS)
        {
            vofa_ms = GetTick();
            DBG_UpdateMbox(motor_on, iq_cmd, adc);
            VofaSend(iq_cmd);
        }
#endif
    }
}

static void SYSCTRL_Configuration(void)
{
    SYSCTRL_HSI_Enable(SYSCTRL_HSIOSC_DIV12);
    SYSCTRL_SysClk_Switch(SYSCTRL_SYSCLKSRC_HSI);
    SYSCTRL_PCLKPRS_Config(SYSCTRL_PCLK_DIV1);
    SystemCoreClock = APP_HCLK_HZ;
    __SYSCTRL_GPIOA_CLK_ENABLE();
    __SYSCTRL_GPIOB_CLK_ENABLE();
    __SYSCTRL_GPIOC_CLK_ENABLE();
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif /* USE_FULL_ASSERT */
