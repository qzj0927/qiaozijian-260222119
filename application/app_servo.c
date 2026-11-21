/**
 * @file    app_servo.c
 * @brief   四轴角度状态与 PWM 输出
 *
 * 注意：SG90 是位置舵机，内部闭环，正常负载下跟随误差 < 1°，
 * 所以"指令角"就是"实际角"的最佳估计。堵转/撞限位会丢步，
 * 因此软限位比机械行程各留 5°。
 */
#include "app_servo.h"

#include "bsp_pwm.h"
#include "servo_map.h"

static int16_t s_angle[APP_SERVO_COUNT];
static bool    s_cfg_ok[APP_SERVO_COUNT];
static bool    s_ready;

void app_servo_init(void)
{
    s_ready = true;

    for (uint32_t i = 0; i < APP_SERVO_COUNT; ++i) {
        const servo_cfg_t *cfg = app_config_servo(i);

        s_cfg_ok[i] = servo_map_cfg_sane(cfg);
        if (!s_cfg_ok[i]) {
            s_ready = false;
        }

        s_angle[i] = 0;
        if (cfg != NULL) {
            bsp_pwm_set_pulse((uint8_t)i, servo_map_angle_to_us(cfg, 0));
        }
    }
}

bool app_servo_ready(void)
{
    return s_ready;
}

bool app_servo_cfg_ok(uint32_t axis)
{
    if (axis >= APP_SERVO_COUNT) {
        return false;
    }
    return s_cfg_ok[axis];
}

int16_t app_servo_set_angle(uint32_t axis, int16_t q_d10)
{
    const servo_cfg_t *cfg;

    if (axis >= APP_SERVO_COUNT) {
        return 0;
    }

    cfg = app_config_servo(axis);
    s_angle[axis] = servo_map_clamp_angle(cfg, q_d10);
    bsp_pwm_set_pulse((uint8_t)axis, servo_map_angle_to_us(cfg, s_angle[axis]));

    return s_angle[axis];
}

int16_t app_servo_angle(uint32_t axis)
{
    if (axis >= APP_SERVO_COUNT) {
        return 0;
    }
    return s_angle[axis];
}

void app_servo_home_all(void)
{
    for (uint32_t i = 0; i < APP_SERVO_COUNT; ++i) {
        const servo_cfg_t *cfg = app_config_servo(i);

        if (cfg != NULL) {
            (void)app_servo_set_angle(i, cfg->home_d10);
        }
    }
}
