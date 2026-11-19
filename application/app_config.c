/**
 * @file    app_config.c
 * @brief   四轴标定表（换机械臂/重新标定只改这里）
 *
 * 标定方法（写进设计说明，答辩会问）：
 *  1. 关节角以"该轴行程中点"为 0°（装配时把舵机装在中立 1500µs，此处即 0°），
 *     这样四轴可以共用同一套 ±限位的表，换机械臂只改数值不改逻辑；
 *  2. 用 500µs / 1500µs / 2500µs 三个脉宽分别输出，量出实际角度，
 *     记下"0° 时是多少 µs"填入 zero_us；
 *  3. 算 us_per_deg = (端点 µs 差) / 端点角度差，取整到 0.1；
 *  4. dir：正方向与舵机正转一致填 +1，否则 -1；
 *  5. angle_min/max：机械干涉前的角度各留 5° 余量；
 *  6. pulse_min/max：以实测能稳定到位、不堵转的范围为准，不要照抄 500/2500。
 *
 * 注意：servo_map_cfg_sane() 会检查两个软限位端点是否严格落在脉宽范围内，
 * 因此不要"贴着" 500/2500 设置限位，两端各留至少 20µs 余量。
 */
#include "app_config.h"

#include <stddef.h>

/* 默认值按"SG90 + 3D 打印四自由度臂"的典型装配填，实测后必须替换。
 * 四轴都以行程中点为 0°：±850 ≈ ±85°，us_per_deg = 10 -> 650~2350µs。 */
static const servo_map_cfg_t k_servo_cfg[ARM_AXIS_COUNT] = {
    /* 底座：±85°，正装，中立 1500µs */
    [ARM_AXIS_BASE] = {
        .pulse_min_us  = 500u,
        .pulse_max_us  = 2500u,
        .zero_us       = 1500u,
        .us_per_deg    = 10.0f,
        .dir           = 1,
        .angle_min_d10 = -850,
        .angle_max_d10 = 850,
        .home_d10      = 0
    },
    /* 肩：±75°，正装；回零 +30° 让臂抬起，避免上电就拍到底座 */
    [ARM_AXIS_SHOULDER] = {
        .pulse_min_us  = 500u,
        .pulse_max_us  = 2500u,
        .zero_us       = 1500u,
        .us_per_deg    = 10.0f,
        .dir           = 1,
        .angle_min_d10 = -750,
        .angle_max_d10 = 750,
        .home_d10      = 300
    },
    /* 肘：±80°，反装（舵机正转时肘关节向下），用于验证 dir=-1 分支 */
    [ARM_AXIS_ELBOW] = {
        .pulse_min_us  = 500u,
        .pulse_max_us  = 2500u,
        .zero_us       = 1500u,
        .us_per_deg    = 10.0f,
        .dir           = -1,
        .angle_min_d10 = -800,
        .angle_max_d10 = 800,
        .home_d10      = 600
    },
    /* 夹爪：0~60°，行程小、精度要求低 */
    [ARM_AXIS_GRIPPER] = {
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

const servo_map_cfg_t *app_config_servo(uint32_t axis)
{
    if (axis >= (uint32_t)ARM_AXIS_COUNT) {
        return NULL;
    }
    return &k_servo_cfg[axis];
}
