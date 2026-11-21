/**
 * @file    app_servo.h
 * @brief   四轴角度状态与输出（T1 的"角度->脉宽"落地）
 */
#ifndef APP_SERVO_H
#define APP_SERVO_H

#include <stdint.h>
#include <stdbool.h>

#include "app_config.h"

/** @brief 上电：校验四轴配置，输出中位 */
void app_servo_init(void);

/** @brief 四轴配置是否全部通过自检 */
bool app_servo_ready(void);

/** @brief 单轴配置自检结果 */
bool app_servo_cfg_ok(uint32_t axis);

/**
 * @brief 设置角度并立即输出（内部做软限位钳位）
 * @return 实际生效的角度（0.1°）
 */
int16_t app_servo_set_angle(uint32_t axis, int16_t q_d10);

/** @brief 读当前角度（0.1°） */
int16_t app_servo_angle(uint32_t axis);

/** @brief 四轴回零 */
void app_servo_home_all(void);

#endif /* APP_SERVO_H */
