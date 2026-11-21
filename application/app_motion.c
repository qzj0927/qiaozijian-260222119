/**
 * @file    app_motion.c
 * @brief   动作调度与 200Hz 插补
 *
 * 为什么用绝对时间（t = now - t0）而不是累加 dt：
 * 主循环偶尔被串口打印拖慢几毫秒，用绝对时间只会让采样点稀疏一点，
 * 曲线不会走慢，也不会累积误差。
 */
#include "app_motion.h"

#include "app_servo.h"
#include "main.h"
#include "motion_profile.h"

#include <math.h>

static struct {
    bool          busy;
    uint32_t      t0_ms;
    motion_plan_t plan;
    int16_t       q_from[APP_SERVO_COUNT];
    int16_t       q_tgt[APP_SERVO_COUNT];
    uint16_t      duration_ms;
} s_motion;

static uint8_t shape_from_profile(uint8_t profile)
{
    switch (profile) {
    case 2u:
        return MOTION_SHAPE_SCURVE;
    case 0u:
        /* T2 不允许阶跃：上位机直接拖拽时常下发 0，这里按梯形执行 */
        return MOTION_SHAPE_TRAP;
    case 1u:
    default:
        return MOTION_SHAPE_TRAP;
    }
}

int app_motion_start(const int16_t q_tgt[APP_SERVO_COUNT], uint16_t duration_ms, uint8_t profile)
{
    float max_delta_d10 = 0.0f;

    if ((q_tgt == NULL) || (!app_servo_ready())) {
        return -1;
    }

    for (uint32_t i = 0; i < APP_SERVO_COUNT; ++i) {
        const int16_t from = app_servo_angle(i);
        float         d;

        s_motion.q_from[i] = from;
        s_motion.q_tgt[i]  = q_tgt[i];

        d = fabsf((float)q_tgt[i] - (float)from);
        if (d > max_delta_d10) {
            max_delta_d10 = d;
        }
    }

    /* 已经在目标上：直接对齐，避免除零 */
    if (max_delta_d10 < 0.05f) {
        for (uint32_t i = 0; i < APP_SERVO_COUNT; ++i) {
            (void)app_servo_set_angle(i, s_motion.q_tgt[i]);
        }
        s_motion.busy        = false;
        s_motion.duration_ms = 0u;
        return 0;
    }

    /* 单位换算：伺服层是 0.1°，规划层的限值单位是 °/s、°/s² */
    s_motion.plan = motion_plan_make(max_delta_d10 * 0.1f,
                                    (float)duration_ms * 0.001f,
                                    (motion_shape_t)shape_from_profile(profile),
                                    APP_MAX_SPEED_DPS,
                                    APP_MAX_ACCEL_DPS2);

    s_motion.t0_ms       = HAL_GetTick();
    s_motion.duration_ms = (uint16_t)((s_motion.plan.duration_s * 1000.0f) + 0.5f);

    if (s_motion.plan.duration_s <= 0.0f) {
        for (uint32_t i = 0; i < APP_SERVO_COUNT; ++i) {
            (void)app_servo_set_angle(i, s_motion.q_tgt[i]);
        }
        s_motion.busy = false;
        return 0;
    }

    s_motion.busy = true;
    return 0;
}

int app_motion_home(void)
{
    int16_t tgt[APP_SERVO_COUNT];

    for (uint32_t i = 0; i < APP_SERVO_COUNT; ++i) {
        const servo_cfg_t *cfg = app_config_servo(i);

        if (cfg == NULL) {
            return -1;
        }
        tgt[i] = cfg->home_d10;
    }

    return app_motion_start(tgt, 0u, 1u);   /* 梯形，按限速取最短 */
}

void app_motion_step(uint32_t now_ms)
{
    float   t;
    float   s;
    int16_t q;

    if (!s_motion.busy) {
        return;
    }

    t = (float)(now_ms - s_motion.t0_ms) * 0.001f;
    s = motion_progress(&s_motion.plan, t);

    for (uint32_t i = 0; i < APP_SERVO_COUNT; ++i) {
        const float from = (float)s_motion.q_from[i];
        const float to   = (float)s_motion.q_tgt[i];
        const float cur  = from + ((to - from) * s);

        q = (int16_t)((cur >= 0.0f) ? (cur + 0.5f) : (cur - 0.5f));
        (void)app_servo_set_angle(i, q);
    }

    if (s >= 1.0f) {
        s_motion.busy = false;
    }
}

void app_motion_abort(void)
{
    s_motion.busy = false;
}

bool app_motion_busy(void)
{
    return s_motion.busy;
}

uint16_t app_motion_duration_ms(void)
{
    return s_motion.duration_ms;
}

void app_motion_init(void)
{
    s_motion.busy        = false;
    s_motion.t0_ms       = 0u;
    s_motion.duration_ms = 0u;
}
