/**
 * @file    BSP_led.h
 * @brief   LED 驱动 (PB09 推挽输出, 高电平点亮)
 */
#ifndef BSP_LED_H
#define BSP_LED_H

#include "cw32l012_gpio.h"
#include "cw32l012_sysctrl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* LED: PB09 推挽输出; 高电平点亮 (与板载 LED1 一致) */
#define BSP_LED1_PIN                  GPIO_PIN_9
#define BSP_LED1_PORT                 CW_GPIOB
#define BSP_LED1_CLK_ENABLE()         __SYSCTRL_GPIOB_CLK_ENABLE()
#define BSP_LED1_AF()                 PB09_AFx_GPIO()
#define BSP_LED1_ON()                 PB09_SETHIGH()
#define BSP_LED1_OFF()                PB09_SETLOW()
#define BSP_LED1_TOG()                PB09_TOG()

/**
 * @brief  初始化 LED (PB09 推挽输出, 默认熄灭)
 */
void BSP_LED_Init(void);

/**
 * @brief  点亮 LED
 */
void BSP_LED_On(void);

/**
 * @brief  熄灭 LED
 */
void BSP_LED_Off(void);

/**
 * @brief  翻转 LED
 */
void BSP_LED_Tog(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_LED_H */
