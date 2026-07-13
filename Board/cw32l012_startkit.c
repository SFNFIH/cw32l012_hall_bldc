/**
 * @file cw32l012_startkit.c
 * @author WHXY
 * @brief
 * @version 0.1
 * @date 2024-08-07
 *
 * @copyright Copyright (c) 2021
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
/******************************************************************************
 * Include files
 ******************************************************************************/
#include "cw32l012_startkit.h"

/* GCC weak attribute definition */
#if defined (__GNUC__) && !defined (__clang__)
    #define __WEAK __attribute__((weak))
#else
    #define __WEAK __weak
#endif

KEY_TypeDef KEY1 = {.Port = BSP_KEY1_PORT, .Pin = BSP_KEY1_PIN};
KEY_TypeDef KEY2 = {.Port = BSP_KEY2_PORT, .Pin = BSP_KEY2_PIN};
LED_TypeDef LED1 = {.Port = BSP_LED1_PORT, .Pin = BSP_LED1_PIN};
LED_TypeDef LED2 = {.Port = BSP_LED2_PORT, .Pin = BSP_LED2_PIN};

/* GCC semihosting suppression & newlib-nano syscalls stubs */
#if defined (__GNUC__) && !defined (__clang__)
    /* Forward declaration */
    int __io_putchar(int ch);

    /* Disable semihosting for GCC */
    void _exit(int x) { while(1); }
    int _kill(int pid, int sig) { (void)pid; (void)sig; return -1; }
    int _getpid(void) { return 1; }
    /* newlib-nano syscalls stubs */
    int _close(int file) { (void)file; return -1; }
    int _fstat(int file, void *st) { (void)file; (void)st; return 0; }
    int _isatty(int file) { (void)file; return 1; }
    int _lseek(int file, int ptr, int dir) { (void)file; (void)ptr; (void)dir; return 0; }
    int _read(int file, char *ptr, int len) { (void)file; (void)ptr; (void)len; return 0; }
    int _write(int file, char *ptr, int len)
    {
        for (int i = 0; i < len; i++) {
            __io_putchar(ptr[i]);
        }
        return len;
    }
#endif

#if defined (__GNUC__) && !defined (__clang__)
    #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
    #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

// 发送单个字符
void UART_SendChar(char ch)
{
    while ((BSP_UART->ISR & UARTx_ISR_TXBUSY_Msk) == UARTx_ISR_TXBUSY_Msk); // 等待发送完成
    BSP_UART->TDR = ch; // 发送字符
}
/*******************************************************************************
 * @brief  重定向C库中Printf函数到UART.
 * @retval None
 * @note   None
 */
__WEAK PUTCHAR_PROTOTYPE
{
    UART_SendChar((char)ch);
    return ch;
}

/*******************************************************************************
  * @brief  板载UART打印初始化
  * @param  pclkFreq: pclk freq
  * @param  baudRate: uart baudrate
  * @retval none
  * @note
  */
void Bsp_Uart_Init(uint32_t pclkFreq, uint32_t baudRate)
{
    uint32_t BRR;
    /* configure the uart txd pin */
    BSP_UART_TXD_PORT_PERIPH_CLK_ENABLE();
    BSP_UART_TXD_AF();
    BSP_UART_TXD_PORT->ANALOG &= ~(BSP_UART_TXD_PIN);

    /* configure the uart rxd pin */
    BSP_UART_RXD_PORT_PERIPH_CLK_ENABLE();
    BSP_UART_RXD_AF();
    BSP_UART_RXD_PORT->ANALOG &= ~(BSP_UART_RXD_PIN);

    /* configure uart */
    BSP_UART_PERIPH_CLK_ENABLE();
    BSP_UART_PERIPH_RST_ENABLE();
    BSP_UART_PERIPH_RST_DISABLE();

    BRR = (pclkFreq + (baudRate >> 1) )/ baudRate;    // 加0.5倍的baudRate，相对于4舍5入
    BSP_UART->BRRI =  BRR >> 4;
    BSP_UART->BRRF =  BRR % 16;
    BSP_UART->CR1  = UARTx_CR1_TXEN_Msk | UARTx_CR1_RXEN_Msk;
}

void Bsp_Uart_Closed(void)
{
    BSP_UART_PERIPH_CLK_DISABLE();

    BSP_UART_TXD_PORT->ANALOG |= (BSP_UART_TXD_PIN);
    BSP_UART_RXD_PORT->ANALOG |= (BSP_UART_RXD_PIN);

    BSP_UART_TXD_PORT_PERIPH_CLK_DISABLE();
    BSP_UART_RXD_PORT_PERIPH_CLK_DISABLE();
}

/*******************************************************************************
  * @brief  板载按键初始化
  * @param  KEY
  * @retval None
  * @note
  */
void Bsp_Key_Init(KEY_TypeDef *KEY)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    GPIO_InitStruct.IT = GPIO_IT_FALLING;    // 下降沿中断使能
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT_PULLUP;
    GPIO_InitStruct.Pins = KEY->Pin;

    GPIO_Init(KEY->Port, &GPIO_InitStruct);

//    NVIC_ClearPendingIRQ(BSP_KEY_IRQn);
//    NVIC_EnableIRQ(BSP_KEY_IRQn);

}


/*******************************************************************************
  * @brief  读取板载按键的状态
  * @param  none
  * @retval 返回板载按键的电平状态
  *
  */
KeyStateTypeDef Bsp_Read_Key(KEY_TypeDef *KEY)
{
    uint32_t currentInput;

    currentInput= !!(KEY->Port->IDR & KEY->Pin);  // 读取引脚状态

    return currentInput ? KEY_STATE_RELEASED : KEY_STATE_PRESSED;
}


/*******************************************************************************
  * @brief  板载LED初始化
  * @param  LED
  * @retval None
  * @note
  */
void Bsp_Led_Init(LED_TypeDef *LED)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pins = LED->Pin;

    GPIO_Init(LED->Port, &GPIO_InitStruct);

    Bsp_Led_Off(LED);
}


/*******************************************************************************
  * @brief  点亮板载LED
  * @param  LED
  * @retval None
  */
void Bsp_Led_On(LED_TypeDef *LED)
{
    LED->Port->BSRR |= LED->Pin;
}


/*******************************************************************************
  * @brief  熄灭板载LED
  * @param  LED
  * @retval None
  */
void Bsp_Led_Off(LED_TypeDef *LED)
{
    LED->Port->BRR |= LED->Pin;
}


/*******************************************************************************
  * @brief  翻转板载LED
  * @param  LED
  * @retval None
  */
void Bsp_Led_Tog(LED_TypeDef *LED)
{
    LED->Port->TOG |= LED->Pin;
}


/*******************************************************************************
  * @brief  板载硬件接口初始化
  * @param  None
  * @retval None
  */
 void Bsp_Init(uint32_t ClkFreq)
 {
     Bsp_Led_Init(&LED1);
     Bsp_Led_Init(&LED2);
     Bsp_Key_Init(&KEY1);
     Bsp_Key_Init(&KEY2);
     Bsp_Uart_Init(ClkFreq, 115200);
 }

/************************ (C) COPYRIGHT  *****END OF FILE*************/
