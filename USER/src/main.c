/**
 * @file    main.c
 * @brief   霍尔六步换相 + ADC 调速 + 按键启停
 * @note    换相在 HALLTIM IRQ; 换相后短空白抑制抖边
 */
#include "../inc/main.h"

#define APP_HCLK_HZ     8000000U
#define APP_PCLK_HZ     APP_HCLK_HZ
#define APP_UART_BAUD   115200U
#define DUTY_MIN        16U
#define DUTY_MAX_CAP    170U

/* #region agent log */
#define DBG_MBOX_MAGIC  0x49B1DB60UL
typedef struct
{
    uint32_t magic;
    uint8_t  motor_on;
    uint8_t  hall;
    uint16_t duty;
    uint16_t adc;
    uint16_t skip_cnt;
    uint32_t step_dt;
    uint8_t  moe;
    uint8_t  dir;
    uint16_t flip;
    uint16_t rej;
    uint16_t fw;
    uint16_t bw;
    uint16_t irq;
    uint16_t ok;
    uint32_t stamp;
} DebugMbox_t;

volatile DebugMbox_t g_dbg_mbox;
static uint32_t s_dbg_stamp = 0U;

static void DBG_UpdateMbox(uint8_t motor_on, uint8_t hall, uint16_t duty,
                           uint16_t adc)
{
    s_dbg_stamp++;
    g_dbg_mbox.magic     = DBG_MBOX_MAGIC;
    g_dbg_mbox.motor_on  = motor_on;
    g_dbg_mbox.hall      = hall;
    g_dbg_mbox.duty      = duty;
    g_dbg_mbox.adc       = adc;
    g_dbg_mbox.skip_cnt  = g_hall_skip_cnt;
    g_dbg_mbox.step_dt   = g_hall_step_dt;
    g_dbg_mbox.moe       = (uint8_t)CW_ATIM->BDTR_f.MOE;
    g_dbg_mbox.dir       = (uint8_t)(g_hall_dir < 0 ? 2U : (uint8_t)g_hall_dir);
    g_dbg_mbox.flip      = g_hall_flip_cnt;
    g_dbg_mbox.rej       = g_hall_rej_cnt;
    g_dbg_mbox.fw        = g_hall_fw_cnt;
    g_dbg_mbox.bw        = g_hall_bw_cnt;
    g_dbg_mbox.irq       = g_hall_irq_cnt;
    g_dbg_mbox.ok        = g_hall_ok_cnt;
    g_dbg_mbox.stamp     = s_dbg_stamp;

    printf("DBG hall=%u duty=%u dir=%d fw=%u bw=%u flip=%u blank=%u ok=%u irq=%u dt=%u\r\n",
           (unsigned int)hall,
           (unsigned int)duty,
           (int)g_hall_dir,
           (unsigned int)g_hall_fw_cnt,
           (unsigned int)g_hall_bw_cnt,
           (unsigned int)g_hall_flip_cnt,
           (unsigned int)g_hall_rej_cnt,
           (unsigned int)g_hall_ok_cnt,
           (unsigned int)g_hall_irq_cnt,
           (unsigned int)g_hall_step_dt);
}
/* #endregion */

static void SYSCTRL_Configuration(void);
static uint16_t AdcToDuty(uint16_t adc);
static void Motor_SetEnable(uint8_t on, uint8_t hall, uint16_t duty, uint16_t adc);

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

static void Motor_SetEnable(uint8_t on, uint8_t hall, uint16_t duty, uint16_t adc)
{
    if (on != 0U)
    {
        BSP_HALLTIM_ResetDirection();
        BSP_MOTOR_SetDuty(duty);
        BSP_MOTOR_Start(hall);
        BSP_LED_On();
        printf("MOTOR ON duty=%u hall=0x%X\r\n",
               (unsigned int)duty, (unsigned int)hall);
    }
    else
    {
        BSP_MOTOR_Stop();
        BSP_LED_Off();
        printf("MOTOR OFF flip=%u blank=%u fw=%u bw=%u ok=%u irq=%u\r\n",
               (unsigned int)g_hall_flip_cnt,
               (unsigned int)g_hall_rej_cnt,
               (unsigned int)g_hall_fw_cnt,
               (unsigned int)g_hall_bw_cnt,
               (unsigned int)g_hall_ok_cnt,
               (unsigned int)g_hall_irq_cnt);
    }
    /* #region agent log */
    DBG_UpdateMbox(on, hall, duty, adc);
    /* #endregion */
}

int main(void)
{
    uint8_t hall;
    uint8_t motor_on = 0U;
    uint16_t adc;
    uint16_t duty;
    uint16_t duty_last = 0xFFFFU;
    KeyEvent_t key_ev;
    static uint32_t s_dbg_div = 0U;
    static uint16_t s_flip_last = 0U;

    SYSCTRL_Configuration();
    InitTick(APP_HCLK_HZ);

    BSP_LED_Init();
    BSP_USART_Init(APP_PCLK_HZ, APP_UART_BAUD);
    BSP_HALLTIM_Init();
    BSP_KEY_Init(APP_PCLK_HZ);
    BSP_ADC_Init();
    BSP_MOTOR_Init(APP_PCLK_HZ);

    hall = BSP_HALLTIM_GetState();
    g_hall_state = hall;

    BSP_ADC_Convert(&adc);
    duty = AdcToDuty(adc);
    duty_last = duty;
    BSP_MOTOR_SetDuty(duty);
    Motor_SetEnable(0U, hall, duty, adc);

    printf("KEY=toggle | IRQ+blank FLT=250us duty_cap=%u\r\n",
           (unsigned int)DUTY_MAX_CAP);

    while (1)
    {
        hall = g_hall_state;

        BSP_ADC_Convert(&adc);
        duty = AdcToDuty(adc);

        key_ev = BSP_KEY_GetEvent();
        if (key_ev == KEY_EVT_PRESS)
        {
            motor_on ^= 1U;
            Motor_SetEnable(motor_on, hall, duty, adc);
            duty_last = duty;
        }

        if (motor_on != 0U)
        {
            if (duty != duty_last)
            {
                duty_last = duty;
                BSP_MOTOR_SetDuty(duty);
            }

            /* #region agent log */
            s_dbg_div++;
            if ((g_hall_flip_cnt != s_flip_last) || (s_dbg_div >= 6000U))
            {
                s_flip_last = g_hall_flip_cnt;
                s_dbg_div = 0U;
                DBG_UpdateMbox(motor_on, g_hall_state, duty, adc);
            }
            /* #endregion */
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
