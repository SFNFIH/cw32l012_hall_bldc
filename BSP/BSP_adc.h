/**
 * @file    BSP_adc.h
 * @brief   ADC1: 反电势 U/V/W + 母线 + 电位器
 * @note    PA03=IN3 OUT_U, PA04=IN4 OUT_V, PA05=IN5 OUT_W,
 *          PA06=IN6 V12, PA07=IN7 调速
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

#define BSP_ADC_BEMF_U_PIN            GPIO_PIN_3
#define BSP_ADC_BEMF_V_PIN            GPIO_PIN_4
#define BSP_ADC_BEMF_W_PIN            GPIO_PIN_5
#define BSP_ADC_VBUS_PIN              GPIO_PIN_6
#define BSP_ADC_DUTY_PIN              GPIO_PIN_7

#define BSP_ADC_BEMF_U_CH             ADC_InputCH3
#define BSP_ADC_BEMF_V_CH             ADC_InputCH4
#define BSP_ADC_BEMF_W_CH             ADC_InputCH5
#define BSP_ADC_VBUS_CH               ADC_InputCH6
#define BSP_ADC_DUTY_CH               ADC_InputCH7

/**
 * @brief  初始化 PA03..PA07 模拟输入, 单通道单次转换
 */
void BSP_ADC_Init(void);

/**
 * @brief  软件触发指定通道一次 (ISR 可调用, 阻塞约数微秒)
 * @param  channel  ADC_InputCHx
 * @return 12-bit 结果; 超时返回上次值/0
 */
uint16_t BSP_ADC_ReadChannel(uint32_t channel);

/**
 * @brief  软件触发一次调速采样
 * @param  duty_raw  输出 IN7 原始值 (可为 NULL)
 */
void BSP_ADC_Convert(uint16_t *duty_raw);

/**
 * @brief  读调速通道原始值 (0~4095)
 */
uint16_t BSP_ADC_ReadDuty(void);

/**
 * @brief  读母线分压 IN6
 */
uint16_t BSP_ADC_ReadVbus(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_ADC_H */
