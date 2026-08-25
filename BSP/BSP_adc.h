/**
 * @file    BSP_adc.h
 * @brief   ADC1: 相电流 + 反电势 + 母线 + 电位器
 * @note    PA00/01/02=IN0/1/2  三相 INA180 (U/V/W)
 *          PA03/04/05=IN3/4/5  反电势 OUT_U/V/W
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

#define BSP_ADC_IU_PIN                GPIO_PIN_0
#define BSP_ADC_IV_PIN                GPIO_PIN_1
#define BSP_ADC_IW_PIN                GPIO_PIN_2
#define BSP_ADC_BEMF_U_PIN            GPIO_PIN_3
#define BSP_ADC_BEMF_V_PIN            GPIO_PIN_4
#define BSP_ADC_BEMF_W_PIN            GPIO_PIN_5
#define BSP_ADC_VBUS_PIN              GPIO_PIN_6
#define BSP_ADC_DUTY_PIN              GPIO_PIN_7

#define BSP_ADC_IU_CH                 ADC_InputCH0
#define BSP_ADC_IV_CH                 ADC_InputCH1
#define BSP_ADC_IW_CH                 ADC_InputCH2
#define BSP_ADC_BEMF_U_CH             ADC_InputCH3
#define BSP_ADC_BEMF_V_CH             ADC_InputCH4
#define BSP_ADC_BEMF_W_CH             ADC_InputCH5
#define BSP_ADC_VBUS_CH               ADC_InputCH6
#define BSP_ADC_DUTY_CH               ADC_InputCH7

void BSP_ADC_Init(void);
uint16_t BSP_ADC_ReadChannel(uint32_t channel);
void BSP_ADC_Convert(uint16_t *duty_raw);
uint16_t BSP_ADC_ReadDuty(void);
uint16_t BSP_ADC_ReadVbus(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_ADC_H */
