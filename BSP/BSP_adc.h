/**
 * @file    BSP_adc.h
 * @brief   ADC1: PA07=IN7 调速 (母线电压功能已关闭)
 */
#ifndef BSP_ADC_H
#define BSP_ADC_H

#include <stdint.h>
#include "cw32l012_gpio.h"
#include "cw32l012_sysctrl.h"
#include "cw32l012_adc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_ADC                       CW_ADC1
#define BSP_ADC_PORT                  CW_GPIOA
#define BSP_ADC_RESOLUTION            4095U   /* 12-bit */

/* PA07 → ADC1_IN7 电位器 */
#define BSP_ADC_DUTY_PIN              GPIO_PIN_7
#define BSP_ADC_DUTY_CH               ADC_InputCH7

/**
 * @brief  初始化 PA07 模拟输入, 单通道单次转换
 */
void BSP_ADC_Init(void);

/**
 * @brief  软件触发一次调速采样
 * @param  duty_raw  输出 IN7 原始值 (可为 NULL)
 */
void BSP_ADC_Convert(uint16_t *duty_raw);

/**
 * @brief  读调速通道原始值 (0~4095)
 */
uint16_t BSP_ADC_ReadDuty(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_ADC_H */
