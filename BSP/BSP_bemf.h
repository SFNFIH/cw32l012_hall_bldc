/**
 * @file    BSP_bemf.h
 * @brief   无感六步: 反电势过零 + 开环强拖启动
 * @note    ATIM 更新中断内采样浮空相, 过零后再延时 30° 换相
 */
#ifndef BSP_BEMF_H
#define BSP_BEMF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    BEMF_ST_IDLE = 0,
    BEMF_ST_ALIGN,
    BEMF_ST_RAMP,
    BEMF_ST_RUN,
    BEMF_ST_FAULT
} BemfState_t;

void BSP_BEMF_Init(void);
void BSP_BEMF_Start(uint16_t duty);
void BSP_BEMF_Stop(void);
void BSP_BEMF_SetDuty(uint16_t duty);
void BSP_BEMF_SetVbus(uint16_t vbus_adc);

/* 由 ATIM_IRQHandler 调用, 每 PWM 周期一次 */
void BSP_BEMF_PwmIrqHandler(void);

BemfState_t BSP_BEMF_GetState(void);
uint8_t     BSP_BEMF_GetStep(void);
uint16_t    BSP_BEMF_GetBemf(void);
uint16_t    BSP_BEMF_GetMid(void);
uint16_t    BSP_BEMF_GetPeriod(void);
uint16_t    BSP_BEMF_GetZcCnt(void);
uint16_t    BSP_BEMF_GetMissCnt(void);
uint32_t    BSP_BEMF_GetTick(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BEMF_H */
