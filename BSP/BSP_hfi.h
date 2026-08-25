/**
 * @file    BSP_hfi.h
 * @brief   高频方波注入无感六步 (饱和脉冲辨识 + ΔI 凸极性换相)
 */
#ifndef BSP_HFI_H
#define BSP_HFI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    HFI_ST_IDLE = 0,
    HFI_ST_IDENT,
    HFI_ST_RAMP,
    HFI_ST_RUN,
    HFI_ST_FAULT
} HfiState_t;

void BSP_HFI_Init(void);
void BSP_HFI_Start(uint16_t duty);
void BSP_HFI_Stop(void);
void BSP_HFI_SetDuty(uint16_t duty);
void BSP_HFI_PwmIrqHandler(void);

HfiState_t BSP_HFI_GetState(void);
uint8_t    BSP_HFI_GetStep(void);
uint16_t   BSP_HFI_GetAmp(void);
uint16_t   BSP_HFI_GetCurrent(void);
uint16_t   BSP_HFI_GetPeriod(void);
uint16_t   BSP_HFI_GetLockCnt(void);
uint16_t   BSP_HFI_GetMissCnt(void);
uint8_t    BSP_HFI_GetIdentStep(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_HFI_H */
