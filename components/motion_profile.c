/**
 * @file    motion_profile.c
 * @brief   归一化速度规划：梯形与 S 曲线
 *
 * 记号：全程在归一化空间里推导（总位移 = 1），最后乘回角度即可。
 *   T  总时长
 *   ta 加（减）速段时长，恒有 ta <= T/2
 *   a  加速度绝对值，v 峰值速度
 *
 * 梯形：单纯形如 s'' = ±a，中间匀速。
 *   a = 1 / (ta * (T - ta))        由总位移 = v(T-ta) 推得
 *   v = a * ta
 *
 * S 曲线：加速度按梯形变化（先线性升到 a_p，再线性降到 0），因此 jerk 有界、
 *   起止加速度为 0。取 ta = T/2（无匀速段的三段式 S 曲线），此时
 *   a_p = 8 / T²，v_p = 2 / T，jerk J = 32 / T³。
 *   若请求时长偏短，按 8/T² <= a_limit 与 2/T <= v_limit 自动延长 T。
 */
#include "motion_profile.h"

#include <math.h>

#define MOTION_EPS_S (1e-4f)

/* ---------------- 内部：分段求值 ---------------- */

static float trap_progress(float t, float T, float ta)
{
    const float a = 1.0f / (ta * (T - ta));

    if (t < ta) {
        return 0.5f * a * t * t;
    }
    if (t < (T - ta)) {
        return 0.5f * a * ta * ta + (a * ta) * (t - ta);
    }
    {
        const float td = T - t;
        return 1.0f - 0.5f * a * td * td;
    }
}

/** S 曲线加速段（含加速度上升、下降两小段），t ∈ [0, ta] */
static float scurve_accel(float t, float ta, float jerk)
{
    const float t1 = 0.5f * ta;       /* 加速度升到峰值的时刻 */
    const float a1 = jerk * t1;       /* 峰值加速度 = J * ta / 2 */
    float u;
    float s1;
    float v1;

    if (t <= t1) {
        /* a = J t, v = J t²/2, s = J t³/6 */
        return jerk * t * t * t / 6.0f;
    }

    /* 第二小段：a 从 a1 线性降到 0，对 v、s 连续积分 */
    u  = t - t1;
    s1 = jerk * t1 * t1 * t1 / 6.0f;
    v1 = 0.5f * jerk * t1 * t1;

    return s1 + (v1 * u) + (0.5f * a1 * u * u) - (jerk * u * u * u / 6.0f);
}

static float scurve_progress(float t, float T, float ta)
{
    /* 由总位移 = 1 反解 jerk：J = 4 / (ta² (T - ta)) */
    const float jerk = 4.0f / (ta * ta * (T - ta));

    if (t <= ta) {
        return scurve_accel(t, ta, jerk);
    }
    if (t >= (T - ta)) {
        return 1.0f - scurve_accel(T - t, ta, jerk);
    }
    {
        const float s_acc = jerk * ta * ta * ta / 8.0f;
        const float v_peak = jerk * ta * ta * 0.25f;
        return s_acc + v_peak * (t - ta);
    }
}

/* ---------------- 对外接口 ---------------- */

motion_plan_t motion_plan_make(float max_delta_deg,
                               float req_duration_s,
                               motion_shape_t shape,
                               float v_limit_dps,
                               float a_limit_dps2)
{
    motion_plan_t plan;

    plan.shape        = shape;
    plan.duration_s   = 0.0f;
    plan.ta_s         = 0.0f;
    plan.v_peak_dps   = 0.0f;
    plan.a_peak_dps2  = 0.0f;

    /* 位移为零：一拍到位，不需要规划 */
    if (max_delta_deg <= MOTION_EPS_S) {
        plan.shape = MOTION_SHAPE_STEP;
        return plan;
    }

    /* 阶跃：仅单轴调试使用。正式动作一律走梯形 / S 曲线，T2 明确禁止阶跃。
     * 注意 req_duration_s <= 0 在协议里表示"按最大速度"，不是阶跃，
     * 这里不能提前返回，否则会把 0x20 duration=0 的指令执行成跳变。 */
    if (shape == MOTION_SHAPE_STEP) {
        plan.shape      = MOTION_SHAPE_STEP;
        plan.v_peak_dps = 0.0f;
        return plan;
    }

    {
        /* 归一化后的限值：把"角度限制"除以最大位移，得到 s 空间的限制 */
        const float a_lim_n = (a_limit_dps2 > 1.0f) ? (a_limit_dps2 / max_delta_deg) : 1e6f;
        const float v_lim_n = (v_limit_dps > 1.0f) ? (v_limit_dps / max_delta_deg) : 1e6f;
        float T = req_duration_s;
        float ta;

        if (shape == MOTION_SHAPE_SCURVE) {
            const float t_for_accel = sqrtf(8.0f / a_lim_n);
            const float t_for_speed = 2.0f / v_lim_n;

            if (T < t_for_accel) {
                T = t_for_accel;
            }
            if (T < t_for_speed) {
                T = t_for_speed;
            }
            ta = 0.5f * T;

            plan.a_peak_dps2 = (8.0f / (T * T)) * max_delta_deg;
            plan.v_peak_dps  = (2.0f / T) * max_delta_deg;
        } else {
            const float t_for_accel = 2.0f * sqrtf(1.0f / a_lim_n);
            float disc;
            float v_peak_n;

            if (T < t_for_accel) {
                T = t_for_accel;
            }

            /* 由 a = 1/(ta(T-ta)) 解 ta 的较小根：加速度顶到限值，余下时间匀速 */
            disc = (T * T) - (4.0f / a_lim_n);
            if (disc < 0.0f) {
                disc = 0.0f;
            }
            ta = 0.5f * (T - sqrtf(disc));
            if (ta < MOTION_EPS_S) {
                ta = MOTION_EPS_S;
            }
            if (ta > (0.5f * T)) {
                ta = 0.5f * T;
            }

            v_peak_n = 1.0f / (T - ta);
            if (v_peak_n > v_lim_n) {
                /* 速度先顶到限值：按限速重算时长 */
                ta = v_lim_n / a_lim_n;
                if (ta > (0.5f * T)) {
                    ta = 0.5f * T;
                }
                T = (1.0f / v_lim_n) + ta;
                if (ta > (0.5f * T)) {
                    ta = 0.5f * T;
                }
                v_peak_n = 1.0f / (T - ta);
            }

            plan.a_peak_dps2 = (1.0f / (ta * (T - ta))) * max_delta_deg;
            plan.v_peak_dps  = v_peak_n * max_delta_deg;
        }

        plan.shape      = shape;
        plan.duration_s = T;
        plan.ta_s       = ta;
    }

    return plan;
}

float motion_progress(const motion_plan_t *plan, float t_s)
{
    if (plan == 0) {
        return 1.0f;
    }
    if (plan->shape == MOTION_SHAPE_STEP) {
        return (t_s > 0.0f) ? 1.0f : 0.0f;
    }
    if (plan->duration_s <= MOTION_EPS_S) {
        return 1.0f;
    }
    if (t_s <= 0.0f) {
        return 0.0f;
    }
    if (t_s >= plan->duration_s) {
        return 1.0f;
    }

    if (plan->shape == MOTION_SHAPE_TRAP) {
        return trap_progress(t_s, plan->duration_s, plan->ta_s);
    }
    return scurve_progress(t_s, plan->duration_s, plan->ta_s);
}

float motion_progress_rate(const motion_plan_t *plan, float t_s)
{
    if ((plan == 0) || (plan->shape == MOTION_SHAPE_STEP)) {
        return 0.0f;
    }
    if ((plan->duration_s <= MOTION_EPS_S) || (t_s <= 0.0f) || (t_s >= plan->duration_s)) {
        return 0.0f;
    }

    if (plan->shape == MOTION_SHAPE_TRAP) {
        const float a = 1.0f / (plan->ta_s * (plan->duration_s - plan->ta_s));

        if (t_s < plan->ta_s) {
            return a * t_s;
        }
        if (t_s < (plan->duration_s - plan->ta_s)) {
            return a * plan->ta_s;
        }
        return a * (plan->duration_s - t_s);
    }

    {
        const float ta = plan->ta_s;
        const float jerk = 4.0f / (ta * ta * (plan->duration_s - ta));
        const float t1 = 0.5f * ta;
        const float a1 = jerk * t1;
        const float t = (t_s <= ta) ? t_s : (plan->duration_s - t_s);
        float u;

        if (t <= t1) {
            return 0.5f * jerk * t * t;
        }
        u = t - t1;
        return (0.5f * jerk * t1 * t1) + (a1 * u) - (0.5f * jerk * u * u);
    }
}
