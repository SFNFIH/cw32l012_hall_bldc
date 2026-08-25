/**
 * @file    BSP_foc.h
 * @brief   无感 FOC: 电流环 + d 轴方波高频注入 PLL
 */
#ifndef BSP_FOC_H
#define BSP_FOC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    FOC_ST_IDLE = 0,
    FOC_ST_ALIGN,
    FOC_ST_IF,
    FOC_ST_RUN,
    FOC_ST_FAULT
} FocState_t;

void BSP_FOC_Init(void);
void BSP_FOC_Start(uint16_t iq_cmd);
void BSP_FOC_Stop(void);
void BSP_FOC_SetIqRef(uint16_t iq_cmd);
void BSP_FOC_PwmIrqHandler(void);

FocState_t BSP_FOC_GetState(void);
uint16_t   BSP_FOC_GetTheta(void);
int16_t    BSP_FOC_GetId(void);
int16_t    BSP_FOC_GetIq(void);
int16_t    BSP_FOC_GetIqRef(void);
int16_t    BSP_FOC_GetDemod(void);
int16_t    BSP_FOC_GetOmega(void);
uint16_t   BSP_FOC_GetTick(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_FOC_H */
