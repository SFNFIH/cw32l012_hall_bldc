/**
 * @file    BSP_foc.c
 * @brief   FOC 电流环 + d 轴方波 HFI 位置 PLL
 *
 * 峰值中断采样三相 INA180 (下桥全开窗). INA180 单向, I = offset - adc
 * (分流上为正对应相电流从电机回流).
 * HFI 电压加在 Id PI 之后, 避免电流环把注入打掉.
 */
#include "BSP_foc.h"
#include "BSP_motor.h"
#include "BSP_adc.h"

#define Q15_MUL(a, b)       ((int16_t)(((int32_t)(a) * (int32_t)(b)) >> 15))
#define SQRT3_2             28378   /* 0.866025 * 32768 */
#define ONE_THIRD           10923
#define ONE_SQRT3           18919
#define V_LIM               22000
#define HFI_V               1800
#define ALIGN_V             3500
#define ALIGN_TICKS         4000U    /* 200 ms @ 20 kHz */
#define IF_TICKS            24000U   /* 1.2 s */
#define IF_W_START          2
#define IF_W_END            48       /* ~14.6 Hz 电 @ 20 kHz */
#define IQ_IF               280
#define IQ_MAX              900
#define OC_LIM              1800
#define HFI_KP              6
#define HFI_KI              1

static const int16_t s_sin[256] = {
        0,    804,   1608,   2410,   3212,   4011,   4808,   5602,
     6393,   7179,   7962,   8739,   9512,  10278,  11039,  11793,
    12539,  13279,  14010,  14732,  15446,  16151,  16846,  17530,
    18204,  18868,  19519,  20159,  20787,  21403,  22005,  22594,
    23170,  23731,  24279,  24811,  25329,  25832,  26319,  26790,
    27245,  27683,  28105,  28510,  28898,  29268,  29621,  29956,
    30273,  30571,  30852,  31113,  31356,  31580,  31785,  31971,
    32137,  32285,  32412,  32521,  32609,  32678,  32728,  32757,
    32767,  32757,  32728,  32678,  32609,  32521,  32412,  32285,
    32137,  31971,  31785,  31580,  31356,  31113,  30852,  30571,
    30273,  29956,  29621,  29268,  28898,  28510,  28105,  27683,
    27245,  26790,  26319,  25832,  25329,  24811,  24279,  23731,
    23170,  22594,  22005,  21403,  20787,  20159,  19519,  18868,
    18204,  17530,  16846,  16151,  15446,  14732,  14010,  13279,
    12539,  11793,  11039,  10278,   9512,   8739,   7962,   7179,
     6393,   5602,   4808,   4011,   3212,   2410,   1608,    804,
        0,   -804,  -1608,  -2410,  -3212,  -4011,  -4808,  -5602,
    -6393,  -7179,  -7962,  -8739,  -9512, -10278, -11039, -11793,
   -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530,
   -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
   -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790,
   -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
   -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971,
   -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
   -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285,
   -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
   -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683,
   -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
   -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868,
   -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
   -12539, -11793, -11039, -10278,  -9512,  -8739,  -7962,  -7179,
    -6393,  -5602,  -4808,  -4011,  -3212,  -2410,  -1608,   -804
};

typedef struct
{
    int16_t kp;
    int16_t ki;
    int32_t i;
    int16_t lim;
} Pi_t;

static volatile FocState_t s_st = FOC_ST_IDLE;
static volatile uint16_t s_theta = 0U;
static volatile int16_t  s_id = 0;
static volatile int16_t  s_iq = 0;
static volatile int16_t  s_iq_ref = 0;
static volatile int16_t  s_demod = 0;
static volatile int16_t  s_omega = 0;
static volatile uint16_t s_tick = 0U;

static int16_t  s_iq_cmd = 0;
static int16_t  s_i_off[3];
static int8_t   s_hf_sign = 1;
static int32_t  s_pll_i = 0;
static int32_t  s_demod_lpf = 0;
static uint16_t s_state_t = 0U;
static int16_t  s_w_if = IF_W_START;
static Pi_t     s_pi_d = { 24, 2, 0, V_LIM };
static Pi_t     s_pi_q = { 24, 2, 0, V_LIM };

static int16_t SinQ15(uint16_t th)
{
    return s_sin[th >> 8];
}

static int16_t CosQ15(uint16_t th)
{
    return s_sin[(uint8_t)((th >> 8) + 64U)];
}

static int16_t Clamp16(int32_t v, int16_t lim)
{
    if (v > lim)
    {
        return lim;
    }
    if (v < -lim)
    {
        return (int16_t)(-lim);
    }
    return (int16_t)v;
}

static int16_t PiUpdate(Pi_t *p, int16_t err)
{
    int32_t out;
    int32_t ilim = ((int32_t)p->lim) << 8;

    p->i += (int32_t)p->ki * err;
    if (p->i > ilim)
    {
        p->i = ilim;
    }
    else if (p->i < -ilim)
    {
        p->i = -ilim;
    }

    out = (((int32_t)p->kp * err) + p->i) >> 8;
    return Clamp16(out, p->lim);
}

static void CalibOffset(void)
{
    uint8_t n;
    uint32_t accu = 0U, accv = 0U, accw = 0U;

    for (n = 0U; n < 8U; n++)
    {
        accu += BSP_ADC_ReadChannel(BSP_ADC_IU_CH);
        accv += BSP_ADC_ReadChannel(BSP_ADC_IV_CH);
        accw += BSP_ADC_ReadChannel(BSP_ADC_IW_CH);
    }
    s_i_off[0] = (int16_t)(accu / 8U);
    s_i_off[1] = (int16_t)(accv / 8U);
    s_i_off[2] = (int16_t)(accw / 8U);
}

static int16_t ReadPhaseI(uint32_t ch, int16_t off)
{
    /* 单向 INA180: 回流 (I_phase<0) 时 adc 升高 → I = offset - adc */
    return (int16_t)((int32_t)off - (int32_t)BSP_ADC_ReadChannel(ch));
}

static void Svpwm(int16_t ual, int16_t ube)
{
    int32_t ua, ub, uc, umax, umin, u0;
    int32_t du, dv, dw;
    const int32_t mid = (int32_t)BSP_MOTOR_DUTY_MID;
    const int32_t arr = (int32_t)BSP_MOTOR_PWM_ARR;

    ua = ual;
    ub = -(ual >> 1) + Q15_MUL(ube, SQRT3_2);
    uc = -(ual >> 1) - Q15_MUL(ube, SQRT3_2);

    umax = ua;
    if (ub > umax) { umax = ub; }
    if (uc > umax) { umax = uc; }
    umin = ua;
    if (ub < umin) { umin = ub; }
    if (uc < umin) { umin = uc; }
    u0 = (umax + umin) >> 1;
    ua -= u0;
    ub -= u0;
    uc -= u0;

    du = mid + ((ua * arr) >> 16);
    dv = mid + ((ub * arr) >> 16);
    dw = mid + ((uc * arr) >> 16);

    if (du < 2) { du = 2; }
    if (dv < 2) { dv = 2; }
    if (dw < 2) { dw = 2; }
    if (du > arr - 2) { du = arr - 2; }
    if (dv > arr - 2) { dv = arr - 2; }
    if (dw > arr - 2) { dw = arr - 2; }

    BSP_MOTOR_SetPhaseDuty((uint16_t)du, (uint16_t)dv, (uint16_t)dw);
}

static void VoltLimit(int16_t *ud, int16_t *uq)
{
    int32_t m2 = (int32_t)(*ud) * (*ud) + (int32_t)(*uq) * (*uq);
    int32_t lim2 = (int32_t)V_LIM * V_LIM;
    if (m2 > lim2)
    {
        /* 粗略缩放, 避免 sqrt */
        *ud = (int16_t)(((int32_t)(*ud) * V_LIM) / 26000);
        *uq = (int16_t)(((int32_t)(*uq) * V_LIM) / 26000);
    }
}

static void EnterFault(void)
{
    s_st = FOC_ST_FAULT;
    BSP_MOTOR_Stop();
}

void BSP_FOC_Init(void)
{
    s_st = FOC_ST_IDLE;
    s_theta = 0U;
    s_iq_cmd = 0;
    s_tick = 0U;
}

void BSP_FOC_Start(uint16_t iq_cmd)
{
    s_iq_cmd = Clamp16((int32_t)iq_cmd, IQ_MAX);
    if (s_iq_cmd < 80)
    {
        s_iq_cmd = 80;
    }
    s_theta = 0U;
    s_pll_i = 0;
    s_demod_lpf = 0;
    s_hf_sign = 1;
    s_w_if = IF_W_START;
    s_state_t = 0U;
    s_tick = 0U;
    s_pi_d.i = 0;
    s_pi_q.i = 0;
    s_iq_ref = 0;

    CalibOffset();
    s_st = FOC_ST_ALIGN;
    BSP_MOTOR_Start();
}

void BSP_FOC_Stop(void)
{
    s_st = FOC_ST_IDLE;
    s_iq_ref = 0;
    BSP_MOTOR_Stop();
}

void BSP_FOC_SetIqRef(uint16_t iq_cmd)
{
    s_iq_cmd = Clamp16((int32_t)iq_cmd, IQ_MAX);
}

void BSP_FOC_PwmIrqHandler(void)
{
    int16_t iu, iv, iw;
    int16_t ial, ibe, id, iq;
    int16_t cs, sn, ud, uq, ual, ube;
    int16_t id_ref, iq_ref;
    int32_t dem;
    uint16_t th;
    FocState_t st;

    if (ATIM_GetITStatus(ATIM_STATE_UIF) == RESET)
    {
        return;
    }
    ATIM_ClearITPendingBit(ATIM_STATE_UIF);

    st = s_st;
    if ((st == FOC_ST_IDLE) || (st == FOC_ST_FAULT))
    {
        return;
    }

    if (BSP_MOTOR_IsPeakUpdate() == 0U)
    {
        return;
    }

    s_tick++;
    s_state_t++;

    iu = ReadPhaseI(BSP_ADC_IU_CH, s_i_off[0]);
    iv = ReadPhaseI(BSP_ADC_IV_CH, s_i_off[1]);
    iw = ReadPhaseI(BSP_ADC_IW_CH, s_i_off[2]);

    if ((iu > OC_LIM) || (iu < -OC_LIM) ||
        (iv > OC_LIM) || (iv < -OC_LIM) ||
        (iw > OC_LIM) || (iw < -OC_LIM))
    {
        EnterFault();
        return;
    }

    ial = (int16_t)((((int32_t)iu * 2 - iv - iw) * ONE_THIRD) >> 15);
    ibe = (int16_t)((((int32_t)iv - iw) * ONE_SQRT3) >> 15);

    th = s_theta;
    cs = CosQ15(th);
    sn = SinQ15(th);
    id = (int16_t)(Q15_MUL(ial, cs) + Q15_MUL(ibe, sn));
    iq = (int16_t)(Q15_MUL(ibe, cs) - Q15_MUL(ial, sn));
    s_id = id;
    s_iq = iq;

    if (st != FOC_ST_ALIGN)
    {
        dem = (int32_t)s_hf_sign * iq;
        s_demod_lpf += (dem - s_demod_lpf) >> 3;
        s_demod = (int16_t)Clamp16(s_demod_lpf, 20000);
        s_pll_i += ((int32_t)HFI_KI * s_demod);
        s_omega = (int16_t)Clamp16(
            (((int32_t)HFI_KP * s_demod) >> 4) + (s_pll_i >> 12), 200);
    }

    if (st == FOC_ST_ALIGN)
    {
        s_theta = 0U;
        ud = ALIGN_V;
        uq = 0;
        if (s_state_t >= ALIGN_TICKS)
        {
            s_st = FOC_ST_IF;
            s_state_t = 0U;
            s_pi_d.i = 0;
            s_pi_q.i = 0;
            s_pll_i = 0;
            s_w_if = IF_W_START;
        }
    }
    else if (st == FOC_ST_IF)
    {
        if (s_w_if < IF_W_END)
        {
            if ((s_state_t & 0x1FFU) == 0U)
            {
                s_w_if++;
            }
        }
        s_theta = (uint16_t)(s_theta + (uint16_t)s_w_if);
        id_ref = 0;
        iq_ref = IQ_IF;
        s_iq_ref = iq_ref;
        ud = PiUpdate(&s_pi_d, (int16_t)(id_ref - id));
        uq = PiUpdate(&s_pi_q, (int16_t)(iq_ref - iq));
        ud = (int16_t)(ud + ((int16_t)s_hf_sign * HFI_V));
        if (s_state_t >= IF_TICKS)
        {
            s_st = FOC_ST_RUN;
            s_state_t = 0U;
        }
    }
    else
    {
        s_theta = (uint16_t)(s_theta + (uint16_t)s_omega);
        id_ref = 0;
        iq_ref = s_iq_cmd;
        s_iq_ref = iq_ref;
        ud = PiUpdate(&s_pi_d, (int16_t)(id_ref - id));
        uq = PiUpdate(&s_pi_q, (int16_t)(iq_ref - iq));
        ud = (int16_t)(ud + ((int16_t)s_hf_sign * HFI_V));
    }

    s_hf_sign = (int8_t)(-s_hf_sign);

    VoltLimit(&ud, &uq);
    ual = (int16_t)(Q15_MUL(ud, CosQ15(s_theta)) - Q15_MUL(uq, SinQ15(s_theta)));
    ube = (int16_t)(Q15_MUL(ud, SinQ15(s_theta)) + Q15_MUL(uq, CosQ15(s_theta)));
    Svpwm(ual, ube);
}

FocState_t BSP_FOC_GetState(void) { return s_st; }
uint16_t   BSP_FOC_GetTheta(void) { return s_theta; }
int16_t    BSP_FOC_GetId(void)    { return s_id; }
int16_t    BSP_FOC_GetIq(void)    { return s_iq; }
int16_t    BSP_FOC_GetIqRef(void) { return s_iq_ref; }
int16_t    BSP_FOC_GetDemod(void) { return s_demod; }
int16_t    BSP_FOC_GetOmega(void) { return s_omega; }
uint16_t   BSP_FOC_GetTick(void)  { return s_tick; }
