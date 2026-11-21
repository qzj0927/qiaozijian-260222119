/**
 * @file    motion_profile.c
 * @brief   归一化速度规划：梯形与 S 曲线
 *
 * 归一化空间里总位移 = 1：
 *   梯形：a = 1/(ta(T-ta))，v = a·ta，ta ≤ T/2
 *   S 曲线：加速度按梯形变化（jerk 有界），取 ta = T/2，
 *           则 a_peak = 8/T²、v_peak = 2/T、jerk = 32/T³
 */
#include "motion_profile.h"

#include <math.h>

#define MOTION_EPS_S 1e-4f

static float trap_progress(float t, float T, float ta)
{
    const float a = 1.0f / (ta * (T - ta));

    if (t < ta) {
        return 0.5f * a * t * t;
    }
    if (t < (T - ta)) {
        return (0.5f * a * ta * ta) + ((a * ta) * (t - ta));
    }
    {
        const float td = T - t;

        return 1.0f - (0.5f * a * td * td);
    }
}

/** S 曲线加速段（加速度先升后降），t ∈ [0, ta] */
static float scurve_accel(float t, float ta, float jerk)
{
    const float t1 = 0.5f * ta;      /* 加速度升到峰值的时刻 */
    const float a1 = jerk * t1;      /* 峰值加速度 = J·ta/2 */
    float       u;
    float       s1;
    float       v1;

    if (t <= t1) {
        /* a = Jt, v = Jt²/2, s = Jt³/6 */
        return jerk * t * t * t / 6.0f;
    }

    u  = t - t1;
    s1 = jerk * t1 * t1 * t1 / 6.0f;
    v1 = 0.5f * jerk * t1 * t1;

    return s1 + (v1 * u) + (0.5f * a1 * u * u) - (jerk * u * u * u / 6.0f);
}

static float scurve_progress(float t, float T, float ta)
{
    /* 由总位移 = 1 反解 jerk：J = 4 / (ta²(T-ta)) */
    const float jerk = 4.0f / (ta * ta * (T - ta));

    if (t <= ta) {
        return scurve_accel(t, ta, jerk);
    }
    if (t >= (T - ta)) {
        return 1.0f - scurve_accel(T - t, ta, jerk);
    }
    {
        const float s_acc  = jerk * ta * ta * ta / 8.0f;
        const float v_peak = jerk * ta * ta * 0.25f;

        return s_acc + (v_peak * (t - ta));
    }
}

motion_plan_t motion_plan_make(float max_delta_deg,
                               float req_duration_s,
                               motion_shape_t shape,
                               float v_limit_dps,
                               float a_limit_dps2)
{
    motion_plan_t plan;

    plan.shape       = shape;
    plan.duration_s  = 0.0f;
    plan.ta_s        = 0.0f;
    plan.v_peak_dps  = 0.0f;
    plan.a_peak_dps2 = 0.0f;

    /* 零位移：一拍到位 */
    if (max_delta_deg <= MOTION_EPS_S) {
        plan.shape = MOTION_SHAPE_STEP;
        return plan;
    }

    /* 阶跃：只给调试用。req<=0 表示"按最大速度"，不是阶跃 */
    if (shape == MOTION_SHAPE_STEP) {
        return plan;
    }

    {
        const float a_lim_n = (a_limit_dps2 > 1.0f) ? (a_limit_dps2 / max_delta_deg) : 1e6f;
        const float v_lim_n = (v_limit_dps > 1.0f) ? (v_limit_dps / max_delta_deg) : 1e6f;
        float T = req_duration_s;
        float ta;

        if (T <= 0.0f) {
            T = MOTION_EPS_S;
        }

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
            float       disc;
            float       v_peak_n;

            if (T < t_for_accel) {
                T = t_for_accel;
            }

            /* 解 a = 1/(ta(T-ta)) 的较小根：加速度顶到限值，余下时间匀速 */
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
    if (plan == NULL) {
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
    if ((plan == NULL) || (plan->shape == MOTION_SHAPE_STEP)) {
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
        const float ta   = plan->ta_s;
        const float jerk = 4.0f / (ta * ta * (plan->duration_s - ta));
        const float t1   = 0.5f * ta;
        const float a1   = jerk * t1;
        const float t    = (t_s <= ta) ? t_s : (plan->duration_s - t_s);
        float       u;

        if (t <= t1) {
            return 0.5f * jerk * t * t;
        }
        u = t - t1;
        return (0.5f * jerk * t1 * t1) + (a1 * u) - (0.5f * jerk * u * u);
    }
}
