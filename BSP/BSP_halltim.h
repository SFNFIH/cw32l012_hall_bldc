/**
 * @file    BSP_halltim.h
 * @brief   HALLTIM 霍尔接口 (原理图: PB02=CH1, PB10=CH2, PB11=CH3)
 */
#ifndef BSP_HALLTIM_H
#define BSP_HALLTIM_H

#include <stdint.h>
#include "cw32l012_gpio.h"
#include "cw32l012_sysctrl.h"
#include "cw32l012_halltim.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_HALLTIM_CH1_PIN           GPIO_PIN_2
#define BSP_HALLTIM_CH1_PORT          CW_GPIOB
#define BSP_HALLTIM_CH1_AF()          PB02_AFx_HALLTIMCH1()

#define BSP_HALLTIM_CH2_PIN           GPIO_PIN_10
#define BSP_HALLTIM_CH2_PORT          CW_GPIOB
#define BSP_HALLTIM_CH2_AF()          PB10_AFx_HALLTIMCH2()

#define BSP_HALLTIM_CH3_PIN           GPIO_PIN_11
#define BSP_HALLTIM_CH3_PORT          CW_GPIOB
#define BSP_HALLTIM_CH3_AF()          PB11_AFx_HALLTIMCH3()

#define BSP_HALLTIM_GPIO_CLK_ENABLE() __SYSCTRL_GPIOB_CLK_ENABLE()
#define BSP_HALLTIM_IRQn              BTIM3_HALLTIM_IRQn

extern volatile uint8_t g_hall_state;

/* #region agent log */
extern volatile uint16_t g_hall_skip_cnt;
extern volatile uint16_t g_hall_step_dt;
extern volatile uint16_t g_hall_fw_cnt;
extern volatile uint16_t g_hall_bw_cnt;
extern volatile uint16_t g_hall_flip_cnt;
extern volatile uint16_t g_hall_rej_cnt;
extern volatile uint16_t g_hall_irq_cnt;
extern volatile uint16_t g_hall_ok_cnt;
extern volatile int8_t   g_hall_dir;
/* #endregion */

void BSP_HALLTIM_Init(void);
uint8_t BSP_HALLTIM_GetState(void);
void BSP_HALLTIM_IRQHandler(void);
uint8_t BSP_HALLTIM_ApplyHall(uint8_t hall, uint8_t do_commutate);
void BSP_HALLTIM_NoteTransition(uint8_t hall);
void BSP_HALLTIM_ResetDirection(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_HALLTIM_H */
