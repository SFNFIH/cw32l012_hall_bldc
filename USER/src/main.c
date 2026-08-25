/**
 * @file    main.c
 * @brief   反电势无感六步 + ADC 调速 + 按键启停
 */
#include "../inc/main.h"

#define APP_HCLK_HZ     8000000U
#define APP_PCLK_HZ     APP_HCLK_HZ
#define APP_UART_BAUD   115200U
#define DUTY_MIN        36U
#define DUTY_MAX_CAP    170U

#define DBG_MBOX_MAGIC  0x49B1DB60UL
typedef struct
{
    uint32_t magic;
    uint8_t  motor_on;
    uint8_t  state;
    uint16_t duty;
    uint16_t adc;
    uint16_t bemf;
    uint16_t mid;
    uint8_t  step;
    uint8_t  hall;
    uint16_t zc;
    uint16_t miss;
    uint16_t period;
    uint8_t  moe;
    uint32_t stamp;
} DebugMbox_t;

volatile DebugMbox_t g_dbg_mbox;
static uint32_t s_dbg_stamp = 0U;

static const char *BemfStateName(BemfState_t st)
{
    switch (st)
    {
        case BEMF_ST_ALIGN: return "ALIGN";
        case BEMF_ST_RAMP:  return "RAMP";
        case BEMF_ST_RUN:   return "RUN";
        case BEMF_ST_FAULT: return "FAULT";
        default:            return "IDLE";
    }
}

static void DBG_UpdateMbox(uint8_t motor_on, uint16_t duty, uint16_t adc)
{
    BemfState_t st = BSP_BEMF_GetState();

    s_dbg_stamp++;
    g_dbg_mbox.magic    = DBG_MBOX_MAGIC;
    g_dbg_mbox.motor_on = motor_on;
    g_dbg_mbox.state    = (uint8_t)st;
    g_dbg_mbox.duty     = duty;
    g_dbg_mbox.adc      = adc;
    g_dbg_mbox.bemf     = BSP_BEMF_GetBemf();
    g_dbg_mbox.mid      = BSP_BEMF_GetMid();
    g_dbg_mbox.step     = BSP_BEMF_GetStep();
    g_dbg_mbox.hall     = g_hall_state;
    g_dbg_mbox.zc       = BSP_BEMF_GetZcCnt();
    g_dbg_mbox.miss     = BSP_BEMF_GetMissCnt();
    g_dbg_mbox.period   = BSP_BEMF_GetPeriod();
    g_dbg_mbox.moe      = (uint8_t)CW_ATIM->BDTR_f.MOE;
    g_dbg_mbox.stamp    = s_dbg_stamp;

    printf("DBG st=%s step=%u hall=%u bemf=%u mid=%u duty=%u zc=%u miss=%u dt=%u\r\n",
           BemfStateName(st),
           (unsigned int)g_dbg_mbox.step,
           (unsigned int)g_dbg_mbox.hall,
           (unsigned int)g_dbg_mbox.bemf,
           (unsigned int)g_dbg_mbox.mid,
           (unsigned int)duty,
           (unsigned int)g_dbg_mbox.zc,
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
        BSP_BEMF_SetVbus(BSP_ADC_ReadVbus());
        BSP_BEMF_Start(duty);
        BSP_LED_On();
        printf("MOTOR ON duty=%u (sensorless BEMF)\r\n", (unsigned int)duty);
    }
    else
    {
        BSP_BEMF_Stop();
        BSP_LED_Off();
        printf("MOTOR OFF st=%s zc=%u miss=%u dt=%u\r\n",
               BemfStateName(BSP_BEMF_GetState()),
               (unsigned int)BSP_BEMF_GetZcCnt(),
               (unsigned int)BSP_BEMF_GetMissCnt(),
               (unsigned int)BSP_BEMF_GetPeriod());
    }
    DBG_UpdateMbox(on, duty, adc);
}

int main(void)
{
    uint8_t motor_on = 0U;
    uint16_t adc;
    uint16_t duty;
    uint16_t duty_last = 0xFFFFU;
    uint16_t vbus;
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
    BSP_BEMF_Init();

    NVIC_DisableIRQ(ATIM_IRQn);
    BSP_ADC_Convert(&adc);
    vbus = BSP_ADC_ReadVbus();
    NVIC_EnableIRQ(ATIM_IRQn);

    duty = AdcToDuty(adc);
    duty_last = duty;
    BSP_MOTOR_SetDuty(duty);
    BSP_BEMF_SetVbus(vbus);
    Motor_SetEnable(0U, duty, adc);

    printf("BEMF sensorless | KEY=toggle | vbus=%u mid=%u duty_cap=%u\r\n",
           (unsigned int)vbus,
           (unsigned int)BSP_BEMF_GetMid(),
           (unsigned int)DUTY_MAX_CAP);

    while (1)
    {
        NVIC_DisableIRQ(ATIM_IRQn);
        BSP_ADC_Convert(&adc);
        vbus = BSP_ADC_ReadVbus();
        NVIC_EnableIRQ(ATIM_IRQn);

        duty = AdcToDuty(adc);
        BSP_BEMF_SetVbus(vbus);

        key_ev = BSP_KEY_GetEvent();
        if (key_ev == KEY_EVT_PRESS)
        {
            if (motor_on != 0U)
            {
                motor_on = 0U;
            }
            else
            {
                motor_on = 1U;
            }
            Motor_SetEnable(motor_on, duty, adc);
            duty_last = duty;
        }

        if (BSP_BEMF_GetState() == BEMF_ST_FAULT)
        {
            if (motor_on != 0U)
            {
                motor_on = 0U;
                BSP_LED_Off();
                printf("MOTOR FAULT miss=%u zc=%u — press KEY to retry\r\n",
                       (unsigned int)BSP_BEMF_GetMissCnt(),
                       (unsigned int)BSP_BEMF_GetZcCnt());
            }
        }

        if (motor_on != 0U)
        {
            if (duty != duty_last)
            {
                duty_last = duty;
                BSP_BEMF_SetDuty(duty);
            }

            s_dbg_div++;
            if ((BSP_BEMF_GetState() != (BemfState_t)s_st_last) || (s_dbg_div >= 4000U))
            {
                s_st_last = (uint8_t)BSP_BEMF_GetState();
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
