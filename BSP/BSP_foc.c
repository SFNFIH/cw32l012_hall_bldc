/**
 * @file    BSP_foc.c
 * @brief   FOC 电流环 + d 轴方波 HFI 位置 PLL
 *
 * 峰值中断采样三相 INA180 (下桥全开窗). INA180 单向, I = offset - adc.
 * sin/cos 用片上 CORDIC (q1.15, 与 ADC 并行).
 * 电压圆限制用 EAU 硬件 sqrt + 有符号除法 (不占用 CORDIC).
 * HFI 电压加在 Id PI 之后, 避免电流环把注入打掉.
 */
#include "BSP_foc.h"
#include "BSP_motor.h"
#include "BSP_adc.h"
#include "cw32l012_cordic.h"
#include "cw32l012_eau.h"

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
#define CORDIC_BUSY_TO      64U
#define EAU_BUSY_TO         96U      /* div 最坏 35 HCLK, sqrt 17 HCLK */

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

/*
 * FOC θ: uint16 0..65535 = 0..2π
 * CORDIC z 单位为 π, q1.15 范围 [-1,1) = [-π, π)
 * 把 θ 当 int16 即把 [π, 2π) 折到 [-π, 0)
 */
static void CordicInit(void)
{
    cordic_init_t init;

    init.func    = CORDIC_FUNC_COS;
    init.scale   = 0U;
    init.format  = CORDIC_FORMAT_Q1_15;
    init.iter    = CORDIC_ITER_16; /* 16 次 ≈ 18 HCLK */
    init.comp    = 0U;
    init.ie      = 0U;
    init.dmaeoc  = 0U;
    init.dmaidle = 0U;
    CORDIC_Init(&init);
}

static void CordicStartSinCos(uint16_t th)
{
    /* 单输入运算: 写 Z 启动; FUNC=cos 时 X=cos(z), Y=sin(z) */
    CW_CORDIC->Z = (int32_t)(int16_t)th;
}

static void CordicReadSinCos(int16_t *cs, int16_t *sn)
{
    uint32_t to = CORDIC_BUSY_TO;

    while ((CW_CORDIC->CSR_f.BUSY != 0U) && (to > 0U))
    {
        to--;
    }

    if (to == 0U)
    {
        *cs = 32767;
        *sn = 0;
        return;
    }

    *cs = (int16_t)CW_CORDIC->X;
    *sn = (int16_t)CW_CORDIC->Y;
}

static int EauWait(void)
{
    uint32_t to = EAU_BUSY_TO;

    while ((CW_EAU->CSR_f.BUSY != 0U) && (to > 0U))
    {
        to--;
    }
    return (to != 0U) ? 1 : 0;
}

/* 开方: MODE=2, 写 DIVIDEND 即启动; 不用 EAU_StartOperation (它会再写 DIVISOR) */
static int EauSqrtU32(uint32_t a, uint32_t *out)
{
    CW_EAU->CSR = (uint32_t)EAU_MODE_SQRT;
    CW_EAU->DIVIDEND = a;
    if (EauWait() == 0)
    {
        return 0;
    }
    *out = CW_EAU->QUOTIENT;
    return 1;
}

/* 有符号除法: 手册 11.5 写 DIVIDEND 再写 DIVISOR 启动 */
static int EauDivS32(int32_t a, int32_t b, int32_t *out)
{
    CW_EAU->CSR = (uint32_t)EAU_MODE_SIGNED_DIV;
    CW_EAU->DIVIDEND = (uint32_t)a;
    CW_EAU->DIVISOR = (uint32_t)b;
    if (EauWait() == 0)
    {
        return 0;
    }
    if ((CW_EAU->CSR_f.ZERO != 0U) || (CW_EAU->CSR_f.OVR != 0U))
    {
        return 0;
    }
    *out = (int32_t)CW_EAU->QUOTIENT;
    return 1;
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
    uint32_t mag;
    int32_t udn;
    int32_t uqn;

    if (m2 <= lim2)
    {
        return;
    }

    /* |u| = sqrt(ud^2+uq^2), 再按 V_LIM/|u| 缩到圆内. 超时则保持原值. */
    if (EauSqrtU32((uint32_t)m2, &mag) == 0)
    {
        return;
    }
    if (mag <= (uint32_t)V_LIM)
    {
        return;
    }
    if ((EauDivS32((int32_t)(*ud) * V_LIM, (int32_t)mag, &udn) == 0) ||
        (EauDivS32((int32_t)(*uq) * V_LIM, (int32_t)mag, &uqn) == 0))
    {
        return;
    }
    *ud = Clamp16(udn, V_LIM);
    *uq = Clamp16(uqn, V_LIM);
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
    CordicInit();
    EAU_Init();
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

    th = s_theta;
    CordicStartSinCos(th);

    iu = ReadPhaseI(BSP_ADC_IU_CH, s_i_off[0]);
    iv = ReadPhaseI(BSP_ADC_IV_CH, s_i_off[1]);
    iw = ReadPhaseI(BSP_ADC_IW_CH, s_i_off[2]);

    CordicReadSinCos(&cs, &sn);

    if ((iu > OC_LIM) || (iu < -OC_LIM) ||
        (iv > OC_LIM) || (iv < -OC_LIM) ||
        (iw > OC_LIM) || (iw < -OC_LIM))
    {
        EnterFault();
        return;
    }

    ial = (int16_t)((((int32_t)iu * 2 - iv - iw) * ONE_THIRD) >> 15);
    ibe = (int16_t)((((int32_t)iv - iw) * ONE_SQRT3) >> 15);

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
    ual = (int16_t)(Q15_MUL(ud, cs) - Q15_MUL(uq, sn));
    ube = (int16_t)(Q15_MUL(ud, sn) + Q15_MUL(uq, cs));
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
