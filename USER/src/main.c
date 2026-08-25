/**
 * @file    main.c
 * @brief   高频注入无感六步 + ADC 调速 + 按键启停
 */
#include "../inc/main.h"

#define APP_HCLK_HZ     8000000U
#define APP_PCLK_HZ     APP_HCLK_HZ
#define APP_UART_BAUD   115200U
#define DUTY_MIN        40U
#define DUTY_MAX_CAP    170U

#define DBG_MBOX_MAGIC  0x49B1DB60UL
typedef struct
{
    uint32_t magic;
    uint8_t  motor_on;
    uint8_t  state;
    uint16_t duty;
    uint16_t adc;
    uint16_t amp;
    uint16_t i_raw;
    uint8_t  step;
    uint8_t  hall;
    uint8_t  ident;
    uint16_t lock;
    uint16_t miss;
    uint16_t period;
    uint8_t  moe;
    uint32_t stamp;
} DebugMbox_t;

volatile DebugMbox_t g_dbg_mbox;
static uint32_t s_dbg_stamp = 0U;

static const char *HfiStateName(HfiState_t st)
{
    switch (st)
    {
        case HFI_ST_IDENT: return "IDENT";
        case HFI_ST_RAMP:  return "RAMP";
        case HFI_ST_RUN:   return "RUN";
        case HFI_ST_FAULT: return "FAULT";
        default:           return "IDLE";
    }
}

static void DBG_UpdateMbox(uint8_t motor_on, uint16_t duty, uint16_t adc)
{
    HfiState_t st = BSP_HFI_GetState();

    s_dbg_stamp++;
    g_dbg_mbox.magic    = DBG_MBOX_MAGIC;
    g_dbg_mbox.motor_on = motor_on;
    g_dbg_mbox.state    = (uint8_t)st;
    g_dbg_mbox.duty     = duty;
    g_dbg_mbox.adc      = adc;
    g_dbg_mbox.amp      = BSP_HFI_GetAmp();
    g_dbg_mbox.i_raw    = BSP_HFI_GetCurrent();
    g_dbg_mbox.step     = BSP_HFI_GetStep();
    g_dbg_mbox.hall     = g_hall_state;
    g_dbg_mbox.ident    = BSP_HFI_GetIdentStep();
    g_dbg_mbox.lock     = BSP_HFI_GetLockCnt();
    g_dbg_mbox.miss     = BSP_HFI_GetMissCnt();
    g_dbg_mbox.period   = BSP_HFI_GetPeriod();
    g_dbg_mbox.moe      = (uint8_t)CW_ATIM->BDTR_f.MOE;
    g_dbg_mbox.stamp    = s_dbg_stamp;

    printf("DBG st=%s step=%u hall=%u ident=%u amp=%u i=%u duty=%u lock=%u miss=%u dt=%u\r\n",
           HfiStateName(st),
           (unsigned int)g_dbg_mbox.step,
           (unsigned int)g_dbg_mbox.hall,
           (unsigned int)g_dbg_mbox.ident,
           (unsigned int)g_dbg_mbox.amp,
           (unsigned int)g_dbg_mbox.i_raw,
           (unsigned int)duty,
           (unsigned int)g_dbg_mbox.lock,
           (unsigned int)g_dbg_mbox.miss,
           (unsigned int)g_dbg_mbox.period);
}

static void SYSCTRL_Configuration(void);
static uint16_t AdcToDuty(uint16_t adc);
static void Motor_SetEnable(uint8_t on, uint16_t duty, uint16_t adc);

void InitTick(uint32_t HclkFreq)
{
    SysTick->LOAD = (HclkFreq / 1000U) - 1U;
    SysTick->VAL  = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_ENABLE_Msk;
}

static uint16_t AdcToDuty(uint16_t adc)
{
    uint32_t duty;

    if (adc > BSP_ADC_RESOLUTION)
    {
        adc = (uint16_t)BSP_ADC_RESOLUTION;
    }

    duty = ((uint32_t)adc * (uint32_t)BSP_MOTOR_PWM_ARR) / BSP_ADC_RESOLUTION;
    if (duty > (uint32_t)DUTY_MAX_CAP)
    {
        duty = (uint32_t)DUTY_MAX_CAP;
    }
    if (duty < DUTY_MIN)
    {
        duty = DUTY_MIN;
    }
    return (uint16_t)duty;
}

static void Motor_SetEnable(uint8_t on, uint16_t duty, uint16_t adc)
{
    if (on != 0U)
    {
        BSP_HFI_Start(duty);
        BSP_LED_On();
        printf("MOTOR ON duty=%u (HFI square-wave)\r\n", (unsigned int)duty);
    }
    else
    {
        BSP_HFI_Stop();
        BSP_LED_Off();
        printf("MOTOR OFF st=%s ident=%u lock=%u miss=%u dt=%u\r\n",
               HfiStateName(BSP_HFI_GetState()),
               (unsigned int)BSP_HFI_GetIdentStep(),
               (unsigned int)BSP_HFI_GetLockCnt(),
               (unsigned int)BSP_HFI_GetMissCnt(),
               (unsigned int)BSP_HFI_GetPeriod());
    }
    DBG_UpdateMbox(on, duty, adc);
}

int main(void)
{
    uint8_t motor_on = 0U;
    uint16_t adc;
    uint16_t duty;
    uint16_t duty_last = 0xFFFFU;
    KeyEvent_t key_ev;
    static uint32_t s_dbg_div = 0U;
    static uint8_t s_st_last = 0xFFU;

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
    BSP_HFI_Init();

    NVIC_DisableIRQ(ATIM_IRQn);
    BSP_ADC_Convert(&adc);
    NVIC_EnableIRQ(ATIM_IRQn);

    duty = AdcToDuty(adc);
    duty_last = duty;
    BSP_MOTOR_SetDuty(duty);
    Motor_SetEnable(0U, duty, adc);

    printf("HFI sensorless | KEY=toggle | duty_cap=%u hf=+/-12\r\n",
           (unsigned int)DUTY_MAX_CAP);

    while (1)
    {
        NVIC_DisableIRQ(ATIM_IRQn);
        BSP_ADC_Convert(&adc);
        NVIC_EnableIRQ(ATIM_IRQn);

        duty = AdcToDuty(adc);

        key_ev = BSP_KEY_GetEvent();
        if (key_ev == KEY_EVT_PRESS)
        {
            motor_on = (motor_on != 0U) ? 0U : 1U;
            Motor_SetEnable(motor_on, duty, adc);
            duty_last = duty;
        }

        if (BSP_HFI_GetState() == HFI_ST_FAULT)
        {
            if (motor_on != 0U)
            {
                motor_on = 0U;
                BSP_LED_Off();
                printf("MOTOR FAULT miss=%u lock=%u — press KEY to retry\r\n",
                       (unsigned int)BSP_HFI_GetMissCnt(),
                       (unsigned int)BSP_HFI_GetLockCnt());
            }
        }

        if (motor_on != 0U)
        {
            if (duty != duty_last)
            {
                duty_last = duty;
                BSP_HFI_SetDuty(duty);
            }

            s_dbg_div++;
            if ((BSP_HFI_GetState() != (HfiState_t)s_st_last) || (s_dbg_div >= 4000U))
            {
                s_st_last = (uint8_t)BSP_HFI_GetState();
                s_dbg_div = 0U;
                DBG_UpdateMbox(motor_on, duty, adc);
            }
        }
        else
        {
            duty_last = duty;
        }
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
