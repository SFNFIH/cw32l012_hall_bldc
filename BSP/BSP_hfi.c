/**
 * @file    BSP_hfi.c
 * @brief   10 kHz 方波注入: 导通回路 |ΔI| ∝ 1/L, 用于静止辨识与换相
 *
 * 六步两两导通时采下桥常开相的 INA180。PWM 周期翻转 ±HF 占空比,
 * 相邻周期电流差的幅值随凸极/饱和变化; d 轴对齐时 L 更小、|ΔI| 更大。
 */
#include "BSP_hfi.h"
#include "BSP_motor.h"
#include "BSP_adc.h"

#define HFI_AMP             12U     /* ±duty @ ARR=199 → 约 10 kHz 方波 */
#define HFI_IDENT_DUTY      48U
#define HFI_IDENT_TICKS     24U     /* 1.2 ms / 矢量 */
#define HFI_IDENT_GAP       10U
#define HFI_START_DUTY      52U
#define HFI_RAMP_START      700U
#define HFI_RAMP_MIN        90U
#define HFI_BLANK           10U
#define HFI_FORCE_MAX       16U
#define HFI_LOCK_NEED       6U
#define HFI_IDENT_MARGIN    12U
#define HFI_AMP_LP          3U      /* IIR: y += (x-y)>>3 */

/*
 * 固件相 0/1/2 = CH1/2/3 = PA08/09/10 = OUT_W/V/U
 * 下桥分流: W→PA02 IN2, V→PA01 IN1, U→PA00 IN0
 */
static const uint32_t s_i_ch[3] = {
    BSP_ADC_IW_CH,
    BSP_ADC_IV_CH,
    BSP_ADC_IU_CH
};

/* 与 motor 正向表一致: 每步下桥常开相 */
static const uint8_t s_low_ph[6] = { 1U, 1U, 0U, 0U, 2U, 2U };

static volatile HfiState_t s_state = HFI_ST_IDLE;
static volatile uint8_t  s_step = 0U;
static volatile uint16_t s_duty = HFI_START_DUTY;
static volatile uint16_t s_amp = 0U;
static volatile uint16_t s_i_raw = 0U;
static volatile uint16_t s_period = HFI_RAMP_START;
static volatile uint16_t s_lock_cnt = 0U;
static volatile uint16_t s_miss_cnt = 0U;
static volatile uint8_t  s_ident_best = 0U;
static volatile uint32_t s_tick = 0U;

static int8_t   s_hf_sign = 1;
static uint16_t s_i_prev = 0U;
static uint8_t  s_have_prev = 0U;
static uint16_t s_i_off[3] = { 0U, 0U, 0U };
static uint16_t s_ident_i[6];
static uint8_t  s_ident_idx = 0U;
static uint8_t  s_ident_phase = 0U; /* 0=pulse, 1=gap */
static uint32_t s_phase_t0 = 0U;
static uint32_t s_step_t0 = 0U;
static uint16_t s_ol_period = HFI_RAMP_START;
static uint16_t s_blank_left = 0U;
static uint16_t s_amp_entry = 0U;
static uint8_t  s_amp_ready = 0U;
static uint8_t  s_lock_streak = 0U;
static uint8_t  s_step_saw_lock = 0U;
static uint8_t  s_force_streak = 0U;
static uint8_t  s_min_hold = 0U;
static uint32_t s_ident_acc = 0U;
static uint8_t  s_ident_n = 0U;

static uint16_t ClampDuty(int32_t d)
{
    if (d < 8)
    {
        d = 8;
    }
    if (d > (int32_t)(BSP_MOTOR_PWM_ARR - 2U))
    {
        d = (int32_t)(BSP_MOTOR_PWM_ARR - 2U);
    }
    return (uint16_t)d;
}

static void ApplyStep(uint8_t step, uint8_t with_hf)
{
    int32_t d;

    s_step = (uint8_t)(step % 6U);
    s_step_t0 = s_tick;
    s_blank_left = HFI_BLANK;
    s_amp_ready = 0U;
    s_have_prev = 0U;
    s_step_saw_lock = 0U;
    BSP_MOTOR_CommutateStep(s_step);

    if (with_hf != 0U)
    {
        d = (int32_t)s_duty + ((s_hf_sign > 0) ? (int32_t)HFI_AMP : -(int32_t)HFI_AMP);
        BSP_MOTOR_SetDuty(ClampDuty(d));
    }
    else
    {
        BSP_MOTOR_SetDuty(HFI_IDENT_DUTY);
    }
}

static void NextStep(uint8_t with_hf)
{
    uint16_t dt = (uint16_t)(s_tick - s_step_t0);
    if (dt < 4U)
    {
        dt = 4U;
    }
    s_period = (uint16_t)(((uint32_t)s_period * 3U + dt) / 4U);
    ApplyStep((uint8_t)(s_step + 1U), with_hf);
}

static void EnterFault(void)
{
    s_state = HFI_ST_FAULT;
    BSP_MOTOR_Stop();
}

static uint16_t SampleLoopCurrent(void)
{
    uint16_t raw;
    uint8_t ph = s_low_ph[s_step];
    int32_t v;

    raw = BSP_ADC_ReadChannel(s_i_ch[ph]);
    s_i_raw = raw;
    v = (int32_t)raw - (int32_t)s_i_off[ph];
    if (v < 0)
    {
        v = 0;
    }
    if (v > 4095)
    {
        v = 4095;
    }
    return (uint16_t)v;
}

static void UpdateAmp(uint16_t i_now)
{
    uint16_t di;
    uint16_t amp = s_amp;

    if (s_have_prev == 0U)
    {
        s_i_prev = i_now;
        s_have_prev = 1U;
        return;
    }

    di = (i_now > s_i_prev) ? (uint16_t)(i_now - s_i_prev)
                            : (uint16_t)(s_i_prev - i_now);
    s_i_prev = i_now;
    amp = (uint16_t)(amp + (((int32_t)di - (int32_t)amp) >> HFI_AMP_LP));
    s_amp = amp;
}

static void ApplyHfToggle(void)
{
    int32_t d;

    s_hf_sign = (int8_t)(-s_hf_sign);
    d = (int32_t)s_duty + ((s_hf_sign > 0) ? (int32_t)HFI_AMP : -(int32_t)HFI_AMP);
    BSP_MOTOR_SetDuty(ClampDuty(d));
}

static void CalibrateOffset(void)
{
    uint8_t p;
    uint8_t n;
    uint32_t acc;

    for (p = 0U; p < 3U; p++)
    {
        acc = 0U;
        for (n = 0U; n < 8U; n++)
        {
            acc += BSP_ADC_ReadChannel(s_i_ch[p]);
        }
        s_i_off[p] = (uint16_t)(acc / 8U);
    }
}

static void IdentFinish(void)
{
    uint8_t i;
    uint8_t best = 0U;
    uint16_t mx = s_ident_i[0];
    uint16_t mn = s_ident_i[0];

    for (i = 1U; i < 6U; i++)
    {
        if (s_ident_i[i] > mx)
        {
            mx = s_ident_i[i];
            best = i;
        }
        if (s_ident_i[i] < mn)
        {
            mn = s_ident_i[i];
        }
    }

    s_ident_best = best;
    /* 饱和最大处 ≈ d 轴, 前移 1 步作起转 q 轴 */
    if ((uint16_t)(mx - mn) < HFI_IDENT_MARGIN)
    {
        s_step = 0U;
    }
    else
    {
        s_step = (uint8_t)((best + 1U) % 6U);
    }

    s_ol_period = HFI_RAMP_START;
    s_period = HFI_RAMP_START;
    s_lock_streak = 0U;
    s_min_hold = 0U;
    s_state = HFI_ST_RAMP;
    ApplyStep(s_step, 1U);
}

void BSP_HFI_Init(void)
{
    s_state = HFI_ST_IDLE;
    s_step = 0U;
    s_amp = 0U;
    s_period = HFI_RAMP_START;
    s_lock_cnt = 0U;
    s_miss_cnt = 0U;
    s_tick = 0U;
}

void BSP_HFI_Start(uint16_t duty)
{
    uint8_t i;

    s_duty = (duty < HFI_START_DUTY) ? HFI_START_DUTY : duty;
    s_hf_sign = 1;
    s_amp = 0U;
    s_lock_cnt = 0U;
    s_miss_cnt = 0U;
    s_force_streak = 0U;
    s_tick = 0U;
    s_ident_idx = 0U;
    s_ident_phase = 0U;
    s_phase_t0 = 0U;
    s_ident_acc = 0U;
    s_ident_n = 0U;
    s_have_prev = 0U;
    for (i = 0U; i < 6U; i++)
    {
        s_ident_i[i] = 0U;
    }

    CalibrateOffset();
    s_state = HFI_ST_IDENT;
    BSP_MOTOR_SetDuty(HFI_IDENT_DUTY);
    ApplyStep(0U, 0U);
    BSP_MOTOR_Start(BSP_MOTOR_StepToHall(0U));
    BSP_MOTOR_SetDuty(HFI_IDENT_DUTY);
}

void BSP_HFI_Stop(void)
{
    s_state = HFI_ST_IDLE;
    BSP_MOTOR_SetDuty(s_duty);
    BSP_MOTOR_Stop();
}

void BSP_HFI_SetDuty(uint16_t duty)
{
    s_duty = duty;
}

void BSP_HFI_PwmIrqHandler(void)
{
    uint16_t i_now;
    uint16_t age;
    uint16_t hyst;
    uint16_t tmin;
    uint16_t tmax;
    HfiState_t st;

    if (ATIM_GetITStatus(ATIM_STATE_UIF) == RESET)
    {
        return;
    }
    ATIM_ClearITPendingBit(ATIM_STATE_UIF);

    s_tick++;
    st = s_state;
    if ((st == HFI_ST_IDLE) || (st == HFI_ST_FAULT))
    {
        return;
    }

    i_now = SampleLoopCurrent();

    if (st == HFI_ST_IDENT)
    {
        if (s_ident_phase == 0U)
        {
            if ((s_tick - s_phase_t0) >= (HFI_IDENT_TICKS / 2U))
            {
                s_ident_acc += i_now;
                s_ident_n++;
            }
            if ((s_tick - s_phase_t0) >= HFI_IDENT_TICKS)
            {
                if (s_ident_n == 0U)
                {
                    s_ident_n = 1U;
                }
                s_ident_i[s_ident_idx] = (uint16_t)(s_ident_acc / s_ident_n);
                s_ident_phase = 1U;
                s_phase_t0 = s_tick;
                BSP_MOTOR_FloatAll();
            }
        }
        else if ((s_tick - s_phase_t0) >= HFI_IDENT_GAP)
        {
            s_ident_idx++;
            if (s_ident_idx >= 6U)
            {
                IdentFinish();
                return;
            }
            s_ident_phase = 0U;
            s_phase_t0 = s_tick;
            s_ident_acc = 0U;
            s_ident_n = 0U;
            ApplyStep(s_ident_idx, 0U);
        }
        return;
    }

    UpdateAmp(i_now);
    ApplyHfToggle();

    if (s_blank_left > 0U)
    {
        s_blank_left--;
        return;
    }

    if (s_amp_ready == 0U)
    {
        s_amp_entry = s_amp;
        s_amp_ready = 1U;
    }

    age = (uint16_t)(s_tick - s_step_t0);
    hyst = (uint16_t)((s_amp_entry >> 4) + 6U);
    tmin = (uint16_t)(s_period / 4U);
    if (tmin < 16U)
    {
        tmin = 16U;
    }
    tmax = (uint16_t)(s_period + (s_period / 2U));
    if (tmax < 40U)
    {
        tmax = 40U;
    }

    if (st == HFI_ST_RAMP)
    {
        if ((age >= tmin) && (s_amp > (uint16_t)(s_amp_entry + hyst)))
        {
            s_step_saw_lock = 1U;
        }

        if (age >= s_ol_period)
        {
            if (s_step_saw_lock != 0U)
            {
                if (s_lock_streak < 0xFFU)
                {
                    s_lock_streak++;
                }
                if (s_lock_cnt < 0xFFFFU)
                {
                    s_lock_cnt++;
                }
            }
            else
            {
                s_lock_streak = 0U;
            }

            s_ol_period = (uint16_t)(s_ol_period - (s_ol_period >> 5));
            if (s_ol_period < HFI_RAMP_MIN)
            {
                s_ol_period = HFI_RAMP_MIN;
            }
            s_period = s_ol_period;
            NextStep(1U);
            if (s_ol_period <= HFI_RAMP_MIN)
            {
                if (s_min_hold < 0xFFU)
                {
                    s_min_hold++;
                }
                if ((s_lock_streak >= HFI_LOCK_NEED) ||
                    ((s_min_hold >= 20U) && (s_lock_cnt >= 3U)))
                {
                    s_state = HFI_ST_RUN;
                    s_force_streak = 0U;
                }
            }
        }
        return;
    }

    /* RUN: |ΔI| 上升 (L 下降, 靠近 d 轴) → 换到下一扇区 */
    if (age < tmin)
    {
        return;
    }

    if (s_amp > (uint16_t)(s_amp_entry + hyst))
    {
        if (s_lock_cnt < 0xFFFFU)
        {
            s_lock_cnt++;
        }
        s_force_streak = 0U;
        NextStep(1U);
        return;
    }

    if (age > tmax)
    {
        if (s_miss_cnt < 0xFFFFU)
        {
            s_miss_cnt++;
        }
        s_force_streak++;
        if (s_force_streak >= HFI_FORCE_MAX)
        {
            EnterFault();
            return;
        }
        NextStep(1U);
    }
}

HfiState_t BSP_HFI_GetState(void)     { return s_state; }
uint8_t    BSP_HFI_GetStep(void)      { return s_step; }
uint16_t   BSP_HFI_GetAmp(void)       { return s_amp; }
uint16_t   BSP_HFI_GetCurrent(void)   { return s_i_raw; }
uint16_t   BSP_HFI_GetPeriod(void)    { return s_period; }
uint16_t   BSP_HFI_GetLockCnt(void)   { return s_lock_cnt; }
uint16_t   BSP_HFI_GetMissCnt(void)   { return s_miss_cnt; }
uint8_t    BSP_HFI_GetIdentStep(void) { return s_ident_best; }
