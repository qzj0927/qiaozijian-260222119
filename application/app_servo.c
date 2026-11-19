/**
 * @file    app_servo.c
 * @brief   四轴角度状态与 PWM 输出
 *
 * 为什么"指令角"就是"实际角"：SG90 是位置舵机，内部闭环，正常负载下
 * 跟随误差 < 1°（验收判据就是 1°）。因此没有编码器也够用；
 * 一旦发生堵转/丢步，角度模型就和真实位置不一致 —— 这是本方案已知的
 * 边界条件，写进设计说明，答辩时主动讲清楚比被问出来好。
 */
#include "app_servo.h"

#include "app_config.h"
#include "bsp_pwm.h"
#include "servo_map.h"

#include <math.h>

typedef struct {
    float   q_cmd_d10;   /**< 当前指令角，0.1° */
    float   q_from_d10;  /**< 本次运动起点，0.1° */
    int16_t q_tgt_d10;   /**< 目标角，0.1° */
    bool    cfg_ok;
} servo_axis_t;

static servo_axis_t s_axis[ARM_AXIS_COUNT];
static bool         s_ready;
static bool         s_limited;

static int16_t round_d10(float q)
{
    return (int16_t)((q >= 0.0f) ? (q + 0.5f) : (q - 0.5f));
}

static void output_axis(uint32_t axis)
{
    const servo_map_cfg_t *cfg = app_config_servo(axis);

    if (cfg == 0) {
        return;
    }
    bsp_pwm_set_pulse((uint8_t)axis, servo_map_angle_to_us(cfg, round_d10(s_axis[axis].q_cmd_d10)));
}

void app_servo_init(void)
{
    s_ready   = true;
    s_limited = false;

    for (uint32_t i = 0; i < (uint32_t)ARM_AXIS_COUNT; ++i) {
        const servo_map_cfg_t *cfg = app_config_servo(i);

        s_axis[i].cfg_ok = servo_map_cfg_sane(cfg);
        if (!s_axis[i].cfg_ok) {
            s_ready = false;
        }

        /* 上电先输出 0°（中位），这是一次性的安全动作：
         * 不做插补，避免"从上次断电位置猛冲到零位"。 */
        s_axis[i].q_cmd_d10  = 0.0f;
        s_axis[i].q_from_d10 = 0.0f;
        s_axis[i].q_tgt_d10  = 0;
        output_axis(i);
    }
}

bool app_servo_ready(void)
{
    return s_ready;
}

bool app_servo_cfg_ok(uint32_t axis)
{
    if (axis >= (uint32_t)ARM_AXIS_COUNT) {
        return false;
    }
    return s_axis[axis].cfg_ok;
}

int16_t app_servo_clamp_target(uint32_t axis, int16_t q_d10, bool *clamped)
{
    const servo_map_cfg_t *cfg;
    int16_t                out;

    if (axis >= (uint32_t)ARM_AXIS_COUNT) {
        return 0;
    }

    cfg = app_config_servo(axis);
    out = servo_map_clamp_angle(cfg, q_d10);

    if (out != q_d10) {
        s_limited = true;
        if (clamped != 0) {
            *clamped = true;
        }
    } else if (clamped != 0) {
        *clamped = false;
    }

    s_axis[axis].q_tgt_d10 = out;
    return out;
}

int16_t app_servo_target(uint32_t axis)
{
    if (axis >= (uint32_t)ARM_AXIS_COUNT) {
        return 0;
    }
    return s_axis[axis].q_tgt_d10;
}

int16_t app_servo_angle(uint32_t axis)
{
    if (axis >= (uint32_t)ARM_AXIS_COUNT) {
        return 0;
    }
    return round_d10(s_axis[axis].q_cmd_d10);
}

void app_servo_begin_move(void)
{
    for (uint32_t i = 0; i < (uint32_t)ARM_AXIS_COUNT; ++i) {
        s_axis[i].q_from_d10 = s_axis[i].q_cmd_d10;
    }
}

void app_servo_apply_progress(float s)
{
    if (s < 0.0f) {
        s = 0.0f;
    }
    if (s > 1.0f) {
        s = 1.0f;
    }

    for (uint32_t i = 0; i < (uint32_t)ARM_AXIS_COUNT; ++i) {
        const float from = s_axis[i].q_from_d10;
        const float to   = (float)s_axis[i].q_tgt_d10;

        s_axis[i].q_cmd_d10 = from + ((to - from) * s);
        output_axis(i);
    }
}

void app_servo_finish_move(void)
{
    for (uint32_t i = 0; i < (uint32_t)ARM_AXIS_COUNT; ++i) {
        s_axis[i].q_cmd_d10 = (float)s_axis[i].q_tgt_d10;
        output_axis(i);
    }
}

void app_servo_abort_move(void)
{
    for (uint32_t i = 0; i < (uint32_t)ARM_AXIS_COUNT; ++i) {
        s_axis[i].q_tgt_d10  = round_d10(s_axis[i].q_cmd_d10);
        s_axis[i].q_from_d10 = s_axis[i].q_cmd_d10;
        output_axis(i);
    }
}

void app_servo_set_home(void)
{
    for (uint32_t i = 0; i < (uint32_t)ARM_AXIS_COUNT; ++i) {
        const servo_map_cfg_t *cfg = app_config_servo(i);

        s_axis[i].q_cmd_d10  = (float)cfg->home_d10;
        s_axis[i].q_from_d10 = (float)cfg->home_d10;
        s_axis[i].q_tgt_d10  = cfg->home_d10;
        output_axis(i);
    }
}

bool app_servo_all_at_target(void)
{
    for (uint32_t i = 0; i < (uint32_t)ARM_AXIS_COUNT; ++i) {
        if (fabsf(s_axis[i].q_cmd_d10 - (float)s_axis[i].q_tgt_d10) > 0.2f) {
            return false;
        }
    }
    return true;
}

bool app_servo_any_limited(void)
{
    return s_limited;
}

void app_servo_clear_limits(void)
{
    s_limited = false;
}
