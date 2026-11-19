/**
 * @file    app_motion.c
 * @brief   动作调度与 200Hz 插补
 *
 * 一次运动的生命周期：
 *   1. app_motion_start()：钳位目标、算出最大位移、规划归一化曲线、记录 t0；
 *   2. 主循环每 5ms 调 app_motion_step()：按"绝对时间"求进度 s 并输出；
 *   3. s 到 1 时对齐目标并清 busy。
 *
 * 为什么用绝对时间 t = now - t0 而不是累加 dt：
 *   主循环偶尔会被 USB 中断、打印拖慢几十微秒到几毫秒。用绝对时间，
 *   抖动只会让采样点稀疏一点，不会让曲线"走慢"，也不会累积误差。
 */
#include "app_motion.h"

#include "app_config.h"
#include "app_servo.h"
#include "bsp_time.h"
#include "motion_profile.h"

#include <math.h>

static struct {
    bool          busy;
    uint32_t      t0_ms;
    motion_plan_t plan;
    uint16_t      duration_ms;
} s_motion;

static void apply_immediate(void)
{
    app_servo_begin_move();
    app_servo_apply_progress(1.0f);
    app_servo_finish_move();
}

void app_motion_init(void)
{
    s_motion.busy        = false;
    s_motion.t0_ms       = 0u;
    s_motion.duration_ms = 0u;
    s_motion.plan        = motion_plan_make(0.0f, 0.0f, MOTION_SHAPE_STEP, 0.0f, 0.0f);
}

static int start_move(const int16_t q_tgt[ARM_AXIS_COUNT],
                      uint16_t    duration_ms,
                      motion_shape_t shape,
                      float       v_limit_dps,
                      float       a_limit_dps2)
{
    int16_t tgt[ARM_AXIS_COUNT];
    float   max_delta = 0.0f;

    if (!app_servo_ready()) {
        return -1;
    }
    if (q_tgt == 0) {
        return -1;
    }

    /* 每次新目标都重置"被限位钳位"标志：bit2 只反映最新一条指令是否越界 */
    app_servo_clear_limits();

    /* 目标一律经 app_servo 钳位，越界目标在这里就被夹住并置标志 */
    for (uint32_t i = 0; i < (uint32_t)ARM_AXIS_COUNT; ++i) {
        tgt[i] = app_servo_clamp_target(i, q_tgt[i], 0);
    }

    for (uint32_t i = 0; i < (uint32_t)ARM_AXIS_COUNT; ++i) {
        const float d = fabsf((float)tgt[i] - (float)app_servo_angle(i));

        if (d > max_delta) {
            max_delta = d;
        }
    }

    /* 四轴都已经在目标上：不需要规划，直接对齐（避免除零和"空转 5ms"） */
    if (max_delta < 0.05f) {
        apply_immediate();
        s_motion.busy        = false;
        s_motion.duration_ms = 0u;
        return 0;
    }

    /* 注意单位：app_servo 里的角度是 0.1°，motion_profile 的限值单位是 °/s、°/s²，
     * 这里必须换算成"度"再传进去，否则限速会被缩小 10 倍（表现为动作慢得不正常）。 */
    s_motion.plan = motion_plan_make(max_delta * 0.1f,
                                    (float)duration_ms * 0.001f,
                                    shape,
                                    v_limit_dps,
                                    a_limit_dps2);
    s_motion.t0_ms       = bsp_time_ms();
    s_motion.duration_ms = (uint16_t)((s_motion.plan.duration_s * 1000.0f) + 0.5f);

    app_servo_begin_move();

    if (s_motion.plan.duration_s <= 0.0f) {
        /* 阶跃：一拍到位 */
        app_servo_apply_progress(1.0f);
        app_servo_finish_move();
        s_motion.busy = false;
        return 0;
    }

    app_servo_apply_progress(0.0f);   /* 同时出发：本拍先把起点写出去 */
    s_motion.busy = true;
    return 0;
}

int app_motion_start(const int16_t q_tgt[ARM_AXIS_COUNT], uint16_t duration_ms, uint8_t profile)
{
    motion_shape_t shape;

    switch (profile) {
    case ARM_PROFILE_SCURVE:
        shape = MOTION_SHAPE_SCURVE;
        break;
    case ARM_PROFILE_STEP:
#if (APP_FORCE_SMOOTH_PROFILE == 1)
        /* 任务书 T2 不允许阶跃：上位机直接拖拽时常下发 0，这里按梯形执行 */
        shape = MOTION_SHAPE_TRAP;
#else
        shape = MOTION_SHAPE_STEP;
#endif
        break;
    case ARM_PROFILE_TRAP:
    default:
        shape = MOTION_SHAPE_TRAP;
        break;
    }

    return start_move(q_tgt, duration_ms, shape, APP_MAX_SPEED_DPS, APP_MAX_ACCEL_DPS2);
}

int app_motion_home(void)
{
    int16_t tgt[ARM_AXIS_COUNT];

    for (uint32_t i = 0; i < (uint32_t)ARM_AXIS_COUNT; ++i) {
        const servo_map_cfg_t *cfg = app_config_servo(i);

        if (cfg == 0) {
            return -1;
        }
        tgt[i] = cfg->home_d10;
    }

    return start_move(tgt, 0u, MOTION_SHAPE_TRAP, APP_HOME_SPEED_DPS, APP_MAX_ACCEL_DPS2);
}

void app_motion_step(uint32_t now_ms)
{
    float t;
    float s;

    if (!s_motion.busy) {
        return;
    }

    t = (float)bsp_time_elapsed(s_motion.t0_ms, now_ms) * 0.001f;
    s = motion_progress(&s_motion.plan, t);

    app_servo_apply_progress(s);

    if (s >= 1.0f) {
        app_servo_finish_move();
        s_motion.busy = false;
    }
}

void app_motion_abort(void)
{
    if (!s_motion.busy) {
        return;
    }
    app_servo_abort_move();
    s_motion.busy = false;
    s_motion.duration_ms = 0u;
}

bool app_motion_busy(void)
{
    return s_motion.busy;
}

uint16_t app_motion_last_duration_ms(void)
{
    return s_motion.duration_ms;
}
