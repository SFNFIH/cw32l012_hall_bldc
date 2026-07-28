/**
 * @file    BSP_usart.c
 * @brief   板载调试串口实现 + printf 重定向
 */
#include "BSP_usart.h"

#if defined (__GNUC__) && !defined (__clang__)
#define __WEAK __attribute__((weak))
#else
#ifndef __WEAK
#define __WEAK __weak
#endif
#endif

void BSP_USART_Init(uint32_t pclk_hz, uint32_t baud)
{
    uint32_t brr;

    /* TX: PC15 */
    BSP_USART_TX_CLK_ENABLE();
    BSP_USART_TX_AF();
    BSP_USART_TX_PORT->ANALOG &= ~(BSP_USART_TX_PIN);

    /* RX: PC14 */
    BSP_USART_RX_CLK_ENABLE();
    BSP_USART_RX_AF();
    BSP_USART_RX_PORT->ANALOG &= ~(BSP_USART_RX_PIN);

    /* UART1 */
    BSP_USART_CLK_ENABLE();
    BSP_USART_RST_ENABLE();
    BSP_USART_RST_DISABLE();

    /* oversampling x16: Baud = UCLK / (16*BRRI + BRRF) */
    brr = (pclk_hz + (baud >> 1)) / baud;
    BSP_USART->BRRI = (uint16_t)(brr >> 4);
    BSP_USART->BRRF = (uint16_t)(brr & 0x0FU);
    BSP_USART->CR1  = UARTx_CR1_TXEN_Msk | UARTx_CR1_RXEN_Msk;
}

void BSP_USART_DeInit(void)
{
    BSP_USART_CLK_DISABLE();

    BSP_USART_TX_PORT->ANALOG |= BSP_USART_TX_PIN;
    BSP_USART_RX_PORT->ANALOG |= BSP_USART_RX_PIN;
}

void BSP_USART_SendByte(uint8_t ch)
{
    while ((BSP_USART->ISR & UARTx_ISR_TXBUSY_Msk) == UARTx_ISR_TXBUSY_Msk)
    {
    }
    BSP_USART->TDR = ch;
}

void BSP_USART_SendString(const char *str)
{
    if (str == NULL)
    {
        return;
    }
    while (*str != '\0')
    {
        if (*str == '\n')
        {
            BSP_USART_SendByte('\r');
        }
        BSP_USART_SendByte((uint8_t)*str++);
    }
}

int BSP_USART_ReceiveByte(uint8_t *ch)
{
    if (ch == NULL)
    {
        return 0;
    }
    if ((BSP_USART->ISR & UARTx_ISR_RC_Msk) == 0U)
    {
        return 0;
    }
    *ch = (uint8_t)BSP_USART->RDR;
    return 1;
}

/* -------------------- printf 重定向 -------------------- */
#if defined (__GNUC__) && !defined (__clang__)
int __io_putchar(int ch);
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

__WEAK PUTCHAR_PROTOTYPE
{
    if (ch == '\n')
    {
        BSP_USART_SendByte('\r');
    }
    BSP_USART_SendByte((uint8_t)ch);
    return ch;
}

#if defined (__GNUC__) && !defined (__clang__)
void _exit(int x)
{
    (void)x;
    while (1)
    {
    }
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    return -1;
}

int _getpid(void)
{
    return 1;
}

int _close(int file)
{
    (void)file;
    return -1;
}

int _fstat(int file, void *st)
{
    (void)file;
    (void)st;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

int _read(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;
    return 0;
}

int _write(int file, char *ptr, int len)
{
    (void)file;
    for (int i = 0; i < len; i++)
    {
        __io_putchar(ptr[i]);
    }
    return len;
}
#endif
