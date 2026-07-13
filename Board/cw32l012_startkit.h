/**
 * @file cw32l012_startkit.h
 * @author WHXY
 * @brief
 * @version 0.1
 * @date 2024-08-07
 *
 * @copyright Copyright (c) 2022
 *
 */
/*******************************************************************************
*
* 代码许可和免责信息
* 武汉芯源半导体有限公司授予您使用所有编程代码示例的非专属的版权许可，您可以由此
* 生成根据您的特定需要而定制的相似功能。根据不能被排除的任何法定保证，武汉芯源半
* 导体有限公司及其程序开发商和供应商对程序或技术支持（如果有）不提供任何明示或暗
* 含的保证或条件，包括但不限于暗含的有关适销性、适用于某种特定用途和非侵权的保证
* 或条件。
* 无论何种情形，武汉芯源半导体有限公司及其程序开发商或供应商均不对下列各项负责，
* 即使被告知其发生的可能性时，也是如此：数据的丢失或损坏；直接的、特别的、附带的
* 或间接的损害，或任何后果性经济损害；或利润、业务、收入、商誉或预期可节省金额的
* 损失。
* 某些司法辖区不允许对直接的、附带的或后果性的损害有任何的排除或限制，因此某些或
* 全部上述排除或限制可能并不适用于您。
*
*******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CW32L012_STARTKIT_H
#define __CW32L012_STARTKIT_H

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include "cw32l012_gpio.h"
#include "cw32l012_sysctrl.h"
#include "cw32l012_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
/* Global type definitions                                                    */
/******************************************************************************/
typedef enum{
    KEY_STATE_PRESSED,  // 按下
    KEY_STATE_RELEASED  // 释放
} KeyStateTypeDef;


typedef struct
{
    GPIO_TypeDef* Port;
    uint32_t      Pin;
}LED_TypeDef, KEY_TypeDef;



/******************************************************************************/
/* Global pre-processor symbols/macros ('#define')                            */
/******************************************************************************/

//StartKit硬件资源配置：

/********************* 配置LED端口信息 ****************************************/
#define BSP_LED1_PIN                              GPIO_PIN_9
#define BSP_LED1_PORT                             CW_GPIOB
#define BSP_LED1_PERIPH_CLK_ENABLE()              __SYSCTRL_GPIOB_CLK_ENABLE()
#define BSP_LED1_PERIPH_CLK_DISABLE()             __SYSCTRL_GPIOB_CLK_DISABLE()
#define BSP_LED1_SETHIGH()                        PB09_SETHIGH()
#define BSP_LED1_SETLOW()                         PB09_SETLOW()
#define BSP_LED1_TOG()                            PB09_TOG()

#define BSP_LED2_PIN                              GPIO_PIN_8
#define BSP_LED2_PORT                             CW_GPIOB
#define BSP_LED2_PERIPH_CLK_ENABLE()              __SYSCTRL_GPIOB_CLK_ENABLE()
#define BSP_LED2_PERIPH_CLK_DISABLE()             __SYSCTRL_GPIOB_CLK_DISABLE()
#define BSP_LED2_SETHIGH()                        PB08_SETHIGH()
#define BSP_LED2_SETLOW()                         PB08_SETLOW()
#define BSP_LED2_TOG()                            PB08_TOG()


/******************* 配置用户按键端口信息 *************************************/
#define BSP_KEY1_PIN                              GPIO_PIN_0
#define BSP_KEY1_PORT                             CW_GPIOA
#define BSP_KEY1_PERIPH_CLK_ENABLE()              __SYSCTRL_GPIOA_CLK_ENABLE()
#define BSP_KEY1_PERIPH_CLK_DISABLE()             __SYSCTRL_GPIOA_CLK_DISABLE()
#define BSP_KEY1_GETVALUE()                       PA00_GETVALUE()
#define BSP_KEY1_IRQn                             GPIOA_IRQn

#define BSP_KEY2_PIN                              GPIO_PIN_1
#define BSP_KEY2_PORT                             CW_GPIOA
#define BSP_KEY2_PERIPH_CLK_ENABLE()              __SYSCTRL_GPIOA_CLK_ENABLE()
#define BSP_KEY2_PERIPH_CLK_DISABLE()             __SYSCTRL_GPIOA_CLK_DISABLE()
#define BSP_KEY2_GETVALUE()                       PA01_GETVALUE()
#define BSP_KEY2_IRQn                             GPIOA_IRQn

/******************* 配置用于Printf的端口和外设和信息 *************************/
#define BSP_UART_TXD_PIN                         GPIO_PIN_8
#define BSP_UART_TXD_PORT                        CW_GPIOA
#define BSP_UART_TXD_AF()                        PA08_AFx_UART3TXD()
#define BSP_UART_TXD_PORT_PERIPH_CLK_ENABLE()    __SYSCTRL_GPIOA_CLK_ENABLE()
#define BSP_UART_TXD_PORT_PERIPH_CLK_DISABLE()   __SYSCTRL_GPIOA_CLK_DISABLE()

#define BSP_UART_RXD_PIN                         GPIO_PIN_9
#define BSP_UART_RXD_PORT                        CW_GPIOA
#define BSP_UART_RXD_IRQn                        GPIOA_IRQn
#define BSP_UART_RXD_AF()                        PA09_AFx_UART3RXD()
#define BSP_UART_RXD_PORT_PERIPH_CLK_ENABLE()    __SYSCTRL_GPIOA_CLK_ENABLE()
#define BSP_UART_RXD_PORT_PERIPH_CLK_DISABLE()   __SYSCTRL_GPIOA_CLK_DISABLE()

#define BSP_UART                                 CW_UART3
#define BSP_UART_IRQn                            UART3_IRQn
#define BSP_UART_PERIPH_CLK_ENABLE()             __SYSCTRL_UART3_CLK_ENABLE()
#define BSP_UART_PERIPH_CLK_DISABLE()            __SYSCTRL_UART3_CLK_DISABLE()
#define BSP_UART_PERIPH_RST_ENABLE()             __SYSCTRL_UART3_RST_ENABLE()
#define BSP_UART_PERIPH_RST_DISABLE()            __SYSCTRL_UART3_RST_DISABLE()


/******************************************************************************/
/* Global macro function                                                      */
/******************************************************************************/


/******************************************************************************/
/* Exported variables ('extern', definition in C source)                      */
/******************************************************************************/
extern LED_TypeDef LED1, LED2;
extern KEY_TypeDef KEY1, KEY2;

/******************************************************************************/
/* Global function prototypes ('extern', definition in C source)              */
/******************************************************************************/
extern void Bsp_Uart_Init(uint32_t pclkFreq, uint32_t baudRate);
extern void Bsp_Uart_Closed(void);

extern void Bsp_Key_Init(KEY_TypeDef *KEY);
extern KeyStateTypeDef Bsp_Read_Key(KEY_TypeDef *KEY);

extern void Bsp_Led_Init(LED_TypeDef *LED);
extern void Bsp_Led_On(LED_TypeDef *LED);
extern void Bsp_Led_Off(LED_TypeDef *LED);
extern void Bsp_Led_Tog(LED_TypeDef *LED);

extern void Bsp_Init(uint32_t ClkFreq);

#ifdef __cplusplus
}
#endif

#endif /* __CW32L012_STARTKIT_H */
