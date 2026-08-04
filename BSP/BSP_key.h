/**
 * @file    BSP_key.h
 * @brief   按键 PC13 (按下=高, 空闲=低, 内部下拉), BTIM1 10ms 非阻塞扫描
 */
#ifndef BSP_KEY_H
#define BSP_KEY_H

#include <stdint.h>
#include "cw32l012_gpio.h"
#include "cw32l012_sysctrl.h"
#include "cw32l012_btim.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_KEY1_PIN              GPIO_PIN_13
#define BSP_KEY1_PORT             CW_GPIOC
#define BSP_KEY1_CLK_ENABLE()     __SYSCTRL_GPIOC_CLK_ENABLE()
#define BSP_KEY1_AF()             PC13_AFx_GPIO()
#define BSP_KEY1_READ()           PC13_GETVALUE()

#define BSP_KEY_BTIM              CW_BTIM1
#define BSP_KEY_BTIM_IRQn         BTIM1_IRQn
#define BSP_KEY_SCAN_MS           10U

typedef enum
{
    KEY_EVT_NONE = 0,
    KEY_EVT_PRESS,
    KEY_EVT_RELEASE
} KeyEvent_t;

/**
 * @brief  初始化 PC13 输入 + BTIM1 10ms 中断扫描
 * @param  pclk_hz  定时器时钟 (通常等于 PCLK)
 */
void BSP_KEY_Init(uint32_t pclk_hz);

/**
 * @brief  按键扫描 (由 BTIM1 溢出中断每 10ms 调用)
 */
void BSP_KEY_Scan(void);

/**
 * @brief  取走一次按键事件 (非阻塞)
 * @retval KEY_EVT_NONE / PRESS / RELEASE
 */
KeyEvent_t BSP_KEY_GetEvent(void);

/**
 * @brief  当前消抖后的按下状态, 1=按下
 */
uint8_t BSP_KEY_IsPressed(void);

/**
 * @brief  BTIM1 中断服务入口
 */
void BSP_KEY_TimerIRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_KEY_H */
