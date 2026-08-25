/**
 * @file    BSP_motor.h
 * @brief   三相互补 PWM (FOC / SVPWM)
 * @note    U=PA10/PB15 CH3, V=PA09/PB14 CH2, W=PA08/PB13 CH1
 */
#ifndef BSP_MOTOR_H
#define BSP_MOTOR_H

#include <stdint.h>
#include "cw32l012_gpio.h"
#include "cw32l012_sysctrl.h"
#include "cw32l012_atim.h"
#include "cw32l012.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 中央对齐, f ≈ PCLK / (2*(ARR+1)), 8 MHz → 20 kHz */
#define BSP_MOTOR_PWM_ARR         199U
#define BSP_MOTOR_PWM_DEADTIME    16U
#define BSP_MOTOR_DUTY_MID        ((BSP_MOTOR_PWM_ARR + 1U) / 2U)

void BSP_MOTOR_Init(uint32_t pclk_hz);
void BSP_MOTOR_Start(void);
void BSP_MOTOR_Stop(void);
void BSP_MOTOR_EnablePwmIrq(void);
uint8_t BSP_MOTOR_IsRunning(void);

/**
 * @brief  写三相占空比 (0..ARR), 映射 U→CH3, V→CH2, W→CH1
 */
void BSP_MOTOR_SetPhaseDuty(uint16_t du, uint16_t dv, uint16_t dw);

/** 1=刚过峰值 (下桥全开, 适合三电阻采样) */
uint8_t BSP_MOTOR_IsPeakUpdate(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_MOTOR_H */
