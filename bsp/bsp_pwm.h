/**
 * @file    bsp_pwm.h
 * @brief   四路 50Hz 舵机 PWM（T1）
 *
 * 时基：TIM3 时钟 84MHz，PSC=83 -> 1MHz（1 tick = 1µs），ARR=19999 -> 20ms = 50Hz
 *      分辨率 1µs，满足任务书"脉宽分辨率优于 1µs"。
 */
#ifndef BSP_PWM_H
#define BSP_PWM_H

#include <stdint.h>

#include "app_config.h"

/** @brief 启动四路 PWM，先输出中位 1500µs（上电安全） */
void bsp_pwm_init(void);

/** @brief 设置一路脉宽（0..3），自动钳到 [500, 2500] */
void bsp_pwm_set_pulse(uint8_t ch, uint16_t us);

/** @brief 一次写四路 */
void bsp_pwm_set_all(const uint16_t us[APP_SERVO_COUNT]);

#endif /* BSP_PWM_H */
