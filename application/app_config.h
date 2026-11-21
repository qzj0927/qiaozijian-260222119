/**
 * @file    app_config.h
 * @brief   全局参数：PWM 电气限制 + 四轴标定表
 *
 * 任务书 T1 要求"角度到脉宽的映射参数写在配置文件或宏里，不散落在代码中"，
 * 所以所有与机械/舵机有关的数字都集中在这里的 k_servo_cfg[] 表里。
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>

#define APP_SERVO_COUNT       4u

/* SG90 电气限制（实测后按标定结果改） */
#define APP_PWM_MIN_US       500u
#define APP_PWM_MAX_US      2500u
#define APP_PWM_SAFE_US     1500u      /* 上电先给中位，避免猛冲 */
#define APP_PWM_PERIOD_US  20000u      /* 50Hz */

/** 单轴标定参数 */
typedef struct {
    uint16_t pulse_min_us;    /**< 实测稳定下限 */
    uint16_t pulse_max_us;    /**< 实测稳定上限 */
    uint16_t zero_us;         /**< 0.0° 对应脉宽 */
    float    us_per_deg;      /**< 每度脉宽 */
    int8_t   dir;             /**< 装配方向 +1 / -1 */
    int16_t  angle_min_d10;   /**< 软限位下限（0.1°） */
    int16_t  angle_max_d10;   /**< 软限位上限（0.1°） */
    int16_t  home_d10;        /**< 回零角度（0.1°） */
} servo_cfg_t;

/** @brief 取某轴标定参数；越界返回 NULL */
const servo_cfg_t *app_config_servo(uint32_t axis);

#endif /* APP_CONFIG_H */
