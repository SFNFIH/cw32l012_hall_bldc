/**
 * @file    BSP_adc.c
 * @brief   ADC1 单通道快采: 反电势 / 母线 / 调速
 */
#include "BSP_adc.h"
#include <stddef.h>

void BSP_ADC_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    ADC_InitTypeDef  adc;
    ADC_ChannelTypeDef ch_fast;
    ADC_ChannelTypeDef ch_slow;

    __SYSCTRL_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.IT   = GPIO_IT_NONE;
    GPIO_InitStruct.Pins = BSP_ADC_IU_PIN | BSP_ADC_IV_PIN | BSP_ADC_IW_PIN |
                           BSP_ADC_BEMF_U_PIN | BSP_ADC_BEMF_V_PIN |
                           BSP_ADC_BEMF_W_PIN | BSP_ADC_VBUS_PIN |
                           BSP_ADC_DUTY_PIN;
    GPIO_Init(BSP_ADC_PORT, &GPIO_InitStruct);

    ch_fast.ADC_InputChannel = BSP_ADC_IU_CH;
    ch_fast.ADC_SampTime     = ADC_SampTime6Clk;
    ch_slow.ADC_InputChannel = BSP_ADC_DUTY_CH;
    ch_slow.ADC_SampTime     = ADC_SampTime12Clk;

    adc.ADC_ClkDiv      = ADC_Clk_Div1;
    adc.ADC_ConvertMode = ADC_ConvertMode_Once;
    adc.ADC_SlaveMod    = ADC_SlaveMode_Disable;
    adc.ADC_SQREns      = ADC_SqrEns0to0;

    adc.ADC_IN0 = ch_fast;
    adc.ADC_IN1 = ch_slow;
    adc.ADC_IN2 = ch_slow;
    adc.ADC_IN3 = ch_slow;
    adc.ADC_IN4 = ch_slow;
    adc.ADC_IN5 = ch_slow;
    adc.ADC_IN6 = ch_slow;
    adc.ADC_IN7 = ch_slow;

    ADC_Init(BSP_ADC, &adc);
    ADC_Enable(BSP_ADC);
}

uint16_t BSP_ADC_ReadChannel(uint32_t channel)
{
    uint32_t timeout = 400U;

    CW_ADC1->SQRCFR_f.SQRCH0 = channel;
    ADC_ClearITPendingBit(BSP_ADC, ADC_IT_EOC | ADC_IT_EOS);
    ADC_SoftwareStartConvCmd(BSP_ADC, ENABLE);

    while ((ADC_GetITStatus(BSP_ADC, ADC_IT_EOS) == RESET) && (timeout > 0U))
    {
        timeout--;
    }

    ADC_ClearITPendingBit(BSP_ADC, ADC_IT_EOC | ADC_IT_EOS);

    if (timeout == 0U)
    {
        return 0U;
    }
    return ADC_GetConversionValue(BSP_ADC, ADC_RESULT_0);
}

void BSP_ADC_Convert(uint16_t *duty_raw)
{
    uint16_t v = BSP_ADC_ReadChannel(BSP_ADC_DUTY_CH);
    if (duty_raw != NULL)
    {
        *duty_raw = v;
    }
}

uint16_t BSP_ADC_ReadDuty(void)
{
    return BSP_ADC_ReadChannel(BSP_ADC_DUTY_CH);
}

uint16_t BSP_ADC_ReadVbus(void)
{
    return BSP_ADC_ReadChannel(BSP_ADC_VBUS_CH);
}
