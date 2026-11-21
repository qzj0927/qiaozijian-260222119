/**
 * @file    app_config.c
 * @brief   四轴标定表（换机械臂只改这里）
 *
 * 约定：关节角以"行程中点"为 0°，装配时把舵机装在中立 1500µs。
 * 下面数值是 SG90 + 3D 打印臂的典型起始值，**必须用实测替换**：
 *   1. 输出 500 / 1500 / 2500µs，量出实际角度；
 *   2. us_per_deg = 端点脉宽差 / 端点角度差；
 *   3. 正方向反了就把 dir 改成 -1；
 *   4. 限位两端各留 5°，脉宽两端各留 20µs 余量。
 */
#include "app_config.h"

#include <stddef.h>

static const servo_cfg_t k_servo_cfg[APP_SERVO_COUNT] = {
    /* 底座：±85° */
    {
        .pulse_min_us  = 500u,
        .pulse_max_us  = 2500u,
        .zero_us       = 1500u,
        .us_per_deg    = 10.0f,
        .dir           = 1,
        .angle_min_d10 = -850,
        .angle_max_d10 = 850,
        .home_d10      = 0
    },
    /* 肩：±75° */
    {
        .pulse_min_us  = 500u,
        .pulse_max_us  = 2500u,
        .zero_us       = 1500u,
        .us_per_deg    = 10.0f,
        .dir           = 1,
        .angle_min_d10 = -750,
        .angle_max_d10 = 750,
        .home_d10      = 300
    },
    /* 肘：±80°，反装 */
    {
        .pulse_min_us  = 500u,
        .pulse_max_us  = 2500u,
        .zero_us       = 1500u,
        .us_per_deg    = 10.0f,
        .dir           = -1,
        .angle_min_d10 = -800,
        .angle_max_d10 = 800,
        .home_d10      = 600
    },
    /* 夹爪：0~60° */
    {
        .pulse_min_us  = 500u,
        .pulse_max_us  = 2500u,
        .zero_us       = 1500u,
        .us_per_deg    = 10.0f,
        .dir           = 1,
        .angle_min_d10 = 0,
        .angle_max_d10 = 600,
        .home_d10      = 200
    }
};

const servo_cfg_t *app_config_servo(uint32_t axis)
{
    if (axis >= APP_SERVO_COUNT) {
        return NULL;
    }
    return &k_servo_cfg[axis];
}
