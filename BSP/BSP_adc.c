/**
 * @file    BSP_adc.c
 * @brief   ADC1_IN7 调速 (母线电压已关闭)
 */
#include "BSP_adc.h"

void BSP_ADC_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    ADC_InitTypeDef  adc;

    __SYSCTRL_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.IT   = GPIO_IT_NONE;
    GPIO_InitStruct.Pins = BSP_ADC_DUTY_PIN;
    GPIO_Init(BSP_ADC_PORT, &GPIO_InitStruct);

    adc.ADC_ClkDiv      = ADC_Clk_Div2;
    adc.ADC_ConvertMode = ADC_ConvertMode_Once;
    adc.ADC_SlaveMod    = ADC_SlaveMode_Disable;
    adc.ADC_SQREns      = ADC_SqrEns0to0;   /* 仅 SQR0 */

    adc.ADC_IN0.ADC_InputChannel = BSP_ADC_DUTY_CH;
    adc.ADC_IN0.ADC_SampTime     = ADC_SampTime54Clk;

    adc.ADC_IN1.ADC_InputChannel = ADC_InputCH0;
    adc.ADC_IN1.ADC_SampTime     = ADC_SampTime6Clk;
    adc.ADC_IN2 = adc.ADC_IN1;
    adc.ADC_IN3 = adc.ADC_IN1;
    adc.ADC_IN4 = adc.ADC_IN1;
    adc.ADC_IN5 = adc.ADC_IN1;
    adc.ADC_IN6 = adc.ADC_IN1;
    adc.ADC_IN7 = adc.ADC_IN1;

    ADC_Init(BSP_ADC, &adc);
    ADC_Enable(BSP_ADC);
}

void BSP_ADC_Convert(uint16_t *duty_raw)
{
    uint32_t timeout = 100000U;

    ADC_ClearITPendingBit(BSP_ADC, ADC_IT_EOC | ADC_IT_EOS);
    ADC_SoftwareStartConvCmd(BSP_ADC, ENABLE);

    while ((ADC_GetITStatus(BSP_ADC, ADC_IT_EOS) == RESET) && (timeout > 0U))
    {
        timeout--;
    }

    ADC_ClearITPendingBit(BSP_ADC, ADC_IT_EOC | ADC_IT_EOS);

    if (duty_raw != NULL)
    {
        *duty_raw = ADC_GetConversionValue(BSP_ADC, ADC_RESULT_0);
    }
}

uint16_t BSP_ADC_ReadDuty(void)
{
    uint16_t duty = 0U;
    BSP_ADC_Convert(&duty);
    return duty;
}
