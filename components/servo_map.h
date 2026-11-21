/**
 * @file    servo_map.h
 * @brief   角度 <-> 脉宽映射（T1）与软限位钳位（T3）
 *
 * 纯计算、不碰 HAL，换平台或做 PC 端单测都可以直接复用。
 */
#ifndef SERVO_MAP_H
#define SERVO_MAP_H

#include <stdint.h>
#include <stdbool.h>

#include "app_config.h"

/** @brief 把角度钳到软限位内 */
int16_t servo_map_clamp_angle(const servo_cfg_t *cfg, int16_t q_d10);

/** @brief 角度是否在软限位内 */
bool servo_map_angle_ok(const servo_cfg_t *cfg, int16_t q_d10);

/**
 * @brief 角度 -> 脉宽（µs）
 * @note  先按软限位钳位，再按 [pulse_min_us, pulse_max_us] 钳位，
 *        因此返回值永远落在标定行程内。
 */
uint16_t servo_map_angle_to_us(const servo_cfg_t *cfg, int16_t q_d10);

/** @brief 配置自检：限位闭合、零位在行程内、限位端点严格落在脉宽范围内 */
bool servo_map_cfg_sane(const servo_cfg_t *cfg);

#endif /* SERVO_MAP_H */
