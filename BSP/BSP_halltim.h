/**
 * @file    BSP_halltim.h
 * @brief   HALLTIM 霍尔接口 (原理图: PB02=CH1, PB10=CH2, PB11=CH3)
 */
#ifndef BSP_HALLTIM_H
#define BSP_HALLTIM_H

#include <stdint.h>
#include "cw32l012_gpio.h"
#include "cw32l012_sysctrl.h"
#include "cw32l012_halltim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CH1: PB02 */
#define BSP_HALLTIM_CH1_PIN           GPIO_PIN_2
#define BSP_HALLTIM_CH1_PORT          CW_GPIOB
#define BSP_HALLTIM_CH1_AF()          PB02_AFx_HALLTIMCH1()

/* CH2: PB10 */
#define BSP_HALLTIM_CH2_PIN           GPIO_PIN_10
#define BSP_HALLTIM_CH2_PORT          CW_GPIOB
#define BSP_HALLTIM_CH2_AF()          PB10_AFx_HALLTIMCH2()

/* CH3: PB11 */
#define BSP_HALLTIM_CH3_PIN           GPIO_PIN_11
#define BSP_HALLTIM_CH3_PORT          CW_GPIOB
#define BSP_HALLTIM_CH3_AF()          PB11_AFx_HALLTIMCH3()

#define BSP_HALLTIM_GPIO_CLK_ENABLE() __SYSCTRL_GPIOB_CLK_ENABLE()
#define BSP_HALLTIM_IRQn              BTIM3_HALLTIM_IRQn

/** 霍尔变化中断更新的滤波状态 (bit0=CH1, bit1=CH2, bit2=CH3) */
extern volatile uint8_t g_hall_state;

/**
 * @brief  初始化 HALLTIM 引脚与外设
 */
void BSP_HALLTIM_Init(void);

/**
 * @brief  读取滤波后霍尔状态 (bit0=CH1F, bit1=CH2F, bit2=CH3F)
 */
uint8_t BSP_HALLTIM_GetState(void);

/**
 * @brief  HALLTIM 中断服务 (由 BTIM3_HALLTIM_IRQHandler 调用)
 */
void BSP_HALLTIM_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_HALLTIM_H */
