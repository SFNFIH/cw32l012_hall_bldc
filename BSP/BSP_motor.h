/**
 * @file    BSP_motor.h
 * @brief   BLDC 六步换相 (ATIM 中央对齐 + 互补 PWM)
 * @note    引脚: PA08/PB13=CH1, PA09/PB14=CH2, PA10/PB15=CH3
 *          上下桥 PWM: 导通上桥相 CH+CHN 互补 PWM, 回流下桥相仅开 CHN
 */
#ifndef BSP_MOTOR_H
#define BSP_MOTOR_H

#include <stdint.h>
#include "cw32l012_gpio.h"
#include "cw32l012_sysctrl.h"
#include "cw32l012_atim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PWM: 中央对齐, f ≈ PCLK / (2 * (ARR+1)), 默认 8MHz → 20kHz */
#define BSP_MOTOR_PWM_ARR         199U
#define BSP_MOTOR_PWM_DEADTIME    16U    /* ~2us @ 8MHz */
#define BSP_MOTOR_DUTY_DEFAULT    40U    /* 起步占空比 (相对 ARR) */

/**
 * @brief  初始化 ATIM 三相互补 PWM, 默认全关断
 * @param  pclk_hz  定时器时钟 (通常等于 PCLK)
 */
void BSP_MOTOR_Init(uint32_t pclk_hz);

/**
 * @brief  启动输出 (使能 MOE), 并按当前霍尔换相
 * @param  hall  滤波后霍尔状态 bit0=CH1,bit1=CH2,bit2=CH3
 */
void BSP_MOTOR_Start(uint8_t hall);

/**
 * @brief  停止输出, 三相浮空
 */
void BSP_MOTOR_Stop(void);

/**
 * @brief  按霍尔状态六步换相 (两两导通, 上下桥 PWM)
 * @param  hall  滤波后霍尔状态
 */
void BSP_MOTOR_Commutate(uint8_t hall);

/**
 * @brief  设置三相占空比 (0 ~ ARR)
 */
void BSP_MOTOR_SetDuty(uint16_t duty);

/**
 * @brief  当前占空比
 */
uint16_t BSP_MOTOR_GetDuty(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_MOTOR_H */
