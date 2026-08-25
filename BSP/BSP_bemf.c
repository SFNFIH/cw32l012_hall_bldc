/**
 * @file    BSP_bemf.c
 * @brief   反电势无感六步 (对齐 → 开环斜坡 → 过零闭环)
 */
#include "BSP_bemf.h"
#include "BSP_motor.h"
#include "BSP_adc.h"

/* PWM 20 kHz → tick = 50 us */
#define BEMF_ALIGN_TICKS      4000U   /* 200 ms */
#define BEMF_RAMP_START       800U    /* 40 ms/步 ≈ 4 Hz 电 */
#define BEMF_RAMP_MIN         80U     /* 4 ms/步 ≈ 42 Hz 电 */
#define BEMF_BLANK_MIN        8U
#define BEMF_HYST             36U
#define BEMF_ZC_NEED          8U
#define BEMF_FORCE_MAX        16U
#define BEMF_DELAY_MIN        8U
#define BEMF_DELAY_MAX        600U
#define BEMF_START_DUTY       50U

/* 分压: BEMF 1/11, Vbus 1/4.3 → Vphase=Vbus/2 时
 * bemf_adc * 220 = vbus_adc * 43 */
#define BEMF_MID_NUM          43U
#define BEMF_MID_DEN          220U
#define BEMF_MID_DEFAULT      677U    /* 12 V 母线时的约值 */

/*
 * 固件相: 0=CH1/PA08, 1=CH2/PA09, 2=CH3/PA10
 * PCB: PA08→HIN3→OUT_W, PA09→HIN2→OUT_V, PA10→HIN1→OUT_U
 */
static const uint8_t s_bemf_ch[3] = {
    BSP_ADC_BEMF_W_CH,
    BSP_ADC_BEMF_V_CH,
    BSP_ADC_BEMF_U_CH
};

/* 与 BSP_MOTOR 正向表一致: 每步浮空相 / 过零方向(1=升沿) */
static const uint8_t s_float_ph[6] = { 2U, 0U, 1U, 2U, 0U, 1U };
static const uint8_t s_zc_rise[6]  = { 1U, 0U, 1U, 0U, 1U, 0U };

static volatile BemfState_t s_state = BEMF_ST_IDLE;
static volatile uint8_t  s_step = 0U;
static volatile uint16_t s_duty = BEMF_START_DUTY;
static volatile uint16_t s_vbus = 0U;
static volatile uint16_t s_mid  = BEMF_MID_DEFAULT;
static volatile uint16_t s_bemf = 0U;
static volatile uint16_t s_zc_cnt = 0U;
static volatile uint16_t s_miss_cnt = 0U;
static volatile uint16_t s_period = BEMF_RAMP_START;
static volatile uint32_t s_tick = 0U;

static uint32_t s_state_t0 = 0U;
static uint16_t s_ol_period = BEMF_RAMP_START;
static uint16_t s_blank_left = 0U;
static uint16_t s_delay = 80U;
static uint16_t s_wait_left = 0U;
static uint8_t  s_armed = 0U;
static uint8_t  s_zc_seen = 0U;
static uint8_t  s_zc_streak = 0U;
static uint8_t  s_force_streak = 0U;
static uint8_t  s_min_hold = 0U;
static uint32_t s_step_t0 = 0U;

static uint16_t MidFromVbus(uint16_t vbus)
{
    uint32_t mid;

    if (vbus < 200U)
    {
        return BEMF_MID_DEFAULT;
    }
    mid = ((uint32_t)vbus * BEMF_MID_NUM) / BEMF_MID_DEN;
    if (mid < 120U)
    {
        mid = 120U;
    }
    if (mid > 1800U)
    {
        mid = 1800U;
    }
    return (uint16_t)mid;
}

static void ApplyStep(uint8_t step)
{
    s_step = (uint8_t)(step % 6U);
    s_step_t0 = s_tick;
    s_blank_left = s_period / 6U;
    if (s_blank_left < BEMF_BLANK_MIN)
    {
        s_blank_left = BEMF_BLANK_MIN;
    }
    if (s_blank_left > (s_period / 3U))
    {
        s_blank_left = (uint16_t)(s_period / 3U);
        if (s_blank_left < BEMF_BLANK_MIN)
        {
            s_blank_left = BEMF_BLANK_MIN;
        }
    }
    s_armed = 0U;
    s_zc_seen = 0U;
    s_wait_left = 0U;
    BSP_MOTOR_CommutateStep(s_step);
}

static void NextStep(void)
{
    uint16_t dt = (uint16_t)(s_tick - s_step_t0);
    if (dt < 4U)
    {
        dt = 4U;
    }
    s_period = dt;
    ApplyStep((uint8_t)(s_step + 1U));
}

static void EnterFault(void)
{
    s_state = BEMF_ST_FAULT;
    BSP_MOTOR_Stop();
}

static void HandleZeroCross(uint16_t t_zc)
{
    uint16_t delay;

    if (s_zc_cnt < 0xFFFFU)
    {
        s_zc_cnt++;
    }

    if (t_zc < BEMF_DELAY_MIN)
    {
        t_zc = BEMF_DELAY_MIN;
    }
    if (t_zc > BEMF_DELAY_MAX)
    {
        t_zc = BEMF_DELAY_MAX;
    }

    s_delay = (uint16_t)(((uint32_t)s_delay * 3U + t_zc) / 4U);
    delay = s_delay;
    if (delay < BEMF_DELAY_MIN)
    {
        delay = BEMF_DELAY_MIN;
    }
    s_wait_left = delay;
    s_zc_seen = 1U;
    s_force_streak = 0U;
}

static uint8_t ZcDetect(uint16_t bemf, uint16_t mid)
{
    uint8_t rise = s_zc_rise[s_step];
    uint16_t lo;
    uint16_t hi;

    lo = (mid > BEMF_HYST) ? (uint16_t)(mid - BEMF_HYST) : 0U;
    hi = (uint16_t)(mid + BEMF_HYST);
    if (hi < mid)
    {
        hi = 4095U;
    }

    if (s_armed == 0U)
    {
        if (rise != 0U)
        {
            if (bemf < lo)
            {
                s_armed = 1U;
            }
        }
        else if (bemf > hi)
        {
            s_armed = 1U;
        }
        return 0U;
    }

    if (rise != 0U)
    {
        return (bemf > hi) ? 1U : 0U;
    }
    return (bemf < lo) ? 1U : 0U;
}

void BSP_BEMF_Init(void)
{
    s_state = BEMF_ST_IDLE;
    s_step = 0U;
    s_mid = BEMF_MID_DEFAULT;
    s_zc_cnt = 0U;
    s_miss_cnt = 0U;
    s_period = BEMF_RAMP_START;
    s_tick = 0U;
}

void BSP_BEMF_Start(uint16_t duty)
{
    s_duty = (duty < BEMF_START_DUTY) ? BEMF_START_DUTY : duty;
    s_vbus = BSP_ADC_ReadVbus();
    s_mid = MidFromVbus(s_vbus);
    s_zc_cnt = 0U;
    s_miss_cnt = 0U;
    s_zc_streak = 0U;
    s_force_streak = 0U;
    s_min_hold = 0U;
    s_ol_period = BEMF_RAMP_START;
    s_period = BEMF_RAMP_START;
    s_delay = (uint16_t)(BEMF_RAMP_START / 2U);
    s_tick = 0U;
    s_state_t0 = 0U;
    s_state = BEMF_ST_ALIGN;

    BSP_MOTOR_SetDuty(s_duty);
    ApplyStep(0U);
    BSP_MOTOR_Start(BSP_MOTOR_StepToHall(0U));
}

void BSP_BEMF_Stop(void)
{
    s_state = BEMF_ST_IDLE;
    BSP_MOTOR_Stop();
}

void BSP_BEMF_SetDuty(uint16_t duty)
{
    s_duty = duty;
    if ((s_state == BEMF_ST_RUN) && (BSP_MOTOR_IsRunning() != 0U))
    {
        BSP_MOTOR_SetDuty(duty);
    }
}

void BSP_BEMF_SetVbus(uint16_t vbus_adc)
{
    s_vbus = vbus_adc;
    s_mid = MidFromVbus(vbus_adc);
}

void BSP_BEMF_PwmIrqHandler(void)
{
    uint16_t bemf;
    uint16_t age;
    BemfState_t st;

    if (ATIM_GetITStatus(ATIM_STATE_UIF) == RESET)
    {
        return;
    }
    ATIM_ClearITPendingBit(ATIM_STATE_UIF);

    s_tick++;
    st = s_state;
    if ((st == BEMF_ST_IDLE) || (st == BEMF_ST_FAULT))
    {
        return;
    }

    bemf = BSP_ADC_ReadChannel(s_bemf_ch[s_float_ph[s_step]]);
    s_bemf = bemf;
    age = (uint16_t)(s_tick - s_step_t0);

    if (s_blank_left > 0U)
    {
        s_blank_left--;
        return;
    }

    if (st == BEMF_ST_ALIGN)
    {
        if ((s_tick - s_state_t0) >= BEMF_ALIGN_TICKS)
        {
            s_state = BEMF_ST_RAMP;
            s_state_t0 = s_tick;
            s_ol_period = BEMF_RAMP_START;
            NextStep();
        }
        return;
    }

    if (st == BEMF_ST_RAMP)
    {
        if ((s_zc_seen == 0U) && (ZcDetect(bemf, s_mid) != 0U))
        {
            uint16_t expect = (uint16_t)(s_ol_period / 2U);
            uint16_t lo = expect / 2U;
            uint16_t hi = expect + (expect / 2U);
            s_zc_seen = 1U;
            if (s_zc_cnt < 0xFFFFU)
            {
                s_zc_cnt++;
            }
            if ((age >= lo) && (age <= hi) && (s_ol_period <= 160U))
            {
                if (s_zc_streak < 0xFFU)
                {
                    s_zc_streak++;
                }
            }
            else
            {
                s_zc_streak = 0U;
            }
        }

        if (age >= s_ol_period)
        {
            s_ol_period = (uint16_t)(s_ol_period - (s_ol_period >> 5));
            if (s_ol_period < BEMF_RAMP_MIN)
            {
                s_ol_period = BEMF_RAMP_MIN;
            }
            s_period = s_ol_period;
            NextStep();

            if (s_ol_period <= BEMF_RAMP_MIN)
            {
                if (s_min_hold < 0xFFU)
                {
                    s_min_hold++;
                }
                if ((s_zc_streak >= BEMF_ZC_NEED) ||
                    ((s_min_hold >= 24U) && (s_zc_cnt >= 4U)))
                {
                    s_state = BEMF_ST_RUN;
                    s_delay = (uint16_t)(s_ol_period / 2U);
                    BSP_MOTOR_SetDuty(s_duty);
                }
            }
        }
        return;
    }

    /* RUN */
    if (s_zc_seen == 0U)
    {
        if (ZcDetect(bemf, s_mid) != 0U)
        {
            HandleZeroCross(age);
        }
        else if (age > (uint16_t)(s_delay * 4U + 40U))
        {
            if (s_miss_cnt < 0xFFFFU)
            {
                s_miss_cnt++;
            }
            s_force_streak++;
            if (s_force_streak >= BEMF_FORCE_MAX)
            {
                EnterFault();
                return;
            }
            NextStep();
        }
        return;
    }

    if (s_wait_left > 0U)
    {
        s_wait_left--;
        if (s_wait_left == 0U)
        {
            NextStep();
        }
    }
}

BemfState_t BSP_BEMF_GetState(void) { return s_state; }
uint8_t     BSP_BEMF_GetStep(void)  { return s_step; }
uint16_t    BSP_BEMF_GetBemf(void)  { return s_bemf; }
uint16_t    BSP_BEMF_GetMid(void)   { return s_mid; }
uint16_t    BSP_BEMF_GetPeriod(void){ return s_period; }
uint16_t    BSP_BEMF_GetZcCnt(void) { return s_zc_cnt; }
uint16_t    BSP_BEMF_GetMissCnt(void){ return s_miss_cnt; }
uint32_t    BSP_BEMF_GetTick(void)  { return s_tick; }
