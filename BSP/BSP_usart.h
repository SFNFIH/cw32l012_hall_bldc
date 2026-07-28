/**
 * @file    BSP_usart.h
 * @brief   板载调试串口 (原理图 U1: UART1, PC15=TXD, PC14=RXD)
 * @note    网表中连接器脚标 TX/RX 按外部适配器命名:
 *          U1-TX(PC14) = MCU UART1_RXD, U1-RX(PC15) = MCU UART1_TXD
 */
#ifndef BSP_USART_H
#define BSP_USART_H

#include <stdint.h>
#include <stdio.h>
#include "cw32l012_gpio.h"
#include "cw32l012_sysctrl.h"
#include "cw32l012_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_USART                     CW_UART1
#define BSP_USART_IRQn                UART1_IRQn
#define BSP_USART_CLK_ENABLE()        __SYSCTRL_UART1_CLK_ENABLE()
#define BSP_USART_CLK_DISABLE()       __SYSCTRL_UART1_CLK_DISABLE()
#define BSP_USART_RST_ENABLE()        __SYSCTRL_UART1_RST_ENABLE()
#define BSP_USART_RST_DISABLE()       __SYSCTRL_UART1_RST_DISABLE()

/* MCU TX: PC15 -> 连接器 RX */
#define BSP_USART_TX_PIN              GPIO_PIN_15
#define BSP_USART_TX_PORT             CW_GPIOC
#define BSP_USART_TX_AF()             PC15_AFx_UART1TXD()
#define BSP_USART_TX_CLK_ENABLE()     __SYSCTRL_GPIOC_CLK_ENABLE()

/* MCU RX: PC14 <- 连接器 TX */
#define BSP_USART_RX_PIN              GPIO_PIN_14
#define BSP_USART_RX_PORT             CW_GPIOC
#define BSP_USART_RX_AF()             PC14_AFx_UART1RXD()
#define BSP_USART_RX_CLK_ENABLE()     __SYSCTRL_GPIOC_CLK_ENABLE()

/**
 * @brief  初始化调试串口 (默认 8N1, 收发使能)
 * @param  pclk_hz  UART 时钟频率 (通常等于 PCLK)
 * @param  baud     波特率, 如 115200
 */
void BSP_USART_Init(uint32_t pclk_hz, uint32_t baud);

/**
 * @brief  关闭串口并释放引脚为模拟
 */
void BSP_USART_DeInit(void);

/**
 * @brief  发送一个字节 (阻塞)
 */
void BSP_USART_SendByte(uint8_t ch);

/**
 * @brief  发送字符串 (阻塞, 遇 '\\0' 结束)
 */
void BSP_USART_SendString(const char *str);

/**
 * @brief  尝试接收一个字节 (非阻塞)
 * @param  ch  接收缓冲
 * @retval 1=收到数据, 0=无数据
 */
int BSP_USART_ReceiveByte(uint8_t *ch);

#ifdef __cplusplus
}
#endif

#endif /* BSP_USART_H */
