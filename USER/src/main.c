/**
 * @file    main.c
 * @brief   HALLTIM 六步换相驱动 (滤波霍尔 + ATIM 互补 PWM)
 */
#include "../inc/main.h"

#define APP_HCLK_HZ     8000000U
#define APP_PCLK_HZ     APP_HCLK_HZ
#define APP_UART_BAUD   115200U

static void SYSCTRL_Configuration(void);
static void Hall_PrintState(uint8_t hall);

void InitTick(uint32_t HclkFreq)
{
    SysTick->LOAD = (HclkFreq / 1000U) - 1U;
    SysTick->VAL  = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_ENABLE_Msk;
}

static void Hall_PrintState(uint8_t hall)
{
    printf("Hall=0x%X  F=%u%u%u  duty=%u\r\n",
           (unsigned int)hall,
           (unsigned int)((hall >> 0) & 1U),
           (unsigned int)((hall >> 1) & 1U),
           (unsigned int)((hall >> 2) & 1U),
           (unsigned int)BSP_MOTOR_GetDuty());
}

int main(void)
{
    uint8_t hall;
    uint8_t hall_last = 0xFFU;

    SYSCTRL_Configuration();
    InitTick(APP_HCLK_HZ);

    BSP_LED_Init();
    BSP_USART_Init(APP_PCLK_HZ, APP_UART_BAUD);
    BSP_HALLTIM_Init();
    BSP_MOTOR_Init(APP_PCLK_HZ);

    hall = BSP_HALLTIM_GetState();
    g_hall_state = hall;
    hall_last = hall;

    BSP_MOTOR_SetDuty(BSP_MOTOR_DUTY_DEFAULT);
    BSP_MOTOR_Start(hall);

    printf("BLDC 6-step ready, duty=%u/ARR=%u\r\n",
           (unsigned int)BSP_MOTOR_GetDuty(),
           (unsigned int)BSP_MOTOR_PWM_ARR);
    Hall_PrintState(hall);

    while (1)
    {
        hall = BSP_HALLTIM_GetState();
        g_hall_state = hall;

        if (hall != hall_last)
        {
            hall_last = hall;
            BSP_MOTOR_Commutate(hall);
            Hall_PrintState(hall);
        }
    }
}

static void SYSCTRL_Configuration(void)
{
    SYSCTRL_HSI_Enable(SYSCTRL_HSIOSC_DIV12);
    SYSCTRL_SysClk_Switch(SYSCTRL_SYSCLKSRC_HSI);
    SYSCTRL_PCLKPRS_Config(SYSCTRL_PCLK_DIV1);
    SystemCoreClock = APP_HCLK_HZ;
    __SYSCTRL_GPIOC_CLK_ENABLE();
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif /* USE_FULL_ASSERT */
