/**
 * @file    app_servo.h
 * @brief   四轴状态模型：当前角、目标角、限位钳位、PWM 输出
 *
 * 与 app_motion 的分工：
 *   app_servo  管"角度是多少、能不能到、怎么输出"，不关心时间；
 *   app_motion 管"什么时候到"，把归一化进度喂进来。
 */
#ifndef APPLICATION_APP_SERVO_H
#define APPLICATION_APP_SERVO_H

#include <stdint.h>
#include <stdbool.h>

#include "arm_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 上电初始化：校验四轴配置并输出中位（0°） */
void app_servo_init(void);

/** @brief 四轴配置是否全部通过自检 */
bool app_servo_ready(void);

/** @brief 单轴配置自检结果，自检打印用 */
bool app_servo_cfg_ok(uint32_t axis);

/**
 * @brief 设置目标角（内部做软限位钳位）
 * @param clamped 非空时，被钳位则置 true
 * @return 钳位后的角度
 */
int16_t app_servo_clamp_target(uint32_t axis, int16_t q_d10, bool *clamped);

/** @brief 读目标角 */
int16_t app_servo_target(uint32_t axis);

/** @brief 读当前指令角（舵机无反馈，指令角即"实际角"的最佳估计） */
int16_t app_servo_angle(uint32_t axis);

/** @brief 记录本次运动起点（必须在发起运动前调用） */
void app_servo_begin_move(void);

/** @brief 按归一化进度 s 输出；s=0 回到起点，s=1 到目标 */
void app_servo_apply_progress(float s);

/** @brief 到达目标：把指令角对齐到目标（消除浮点累计误差） */
void app_servo_finish_move(void);

/** @brief 中止运动：停在当前角并取消目标（超时/故障用） */
void app_servo_abort_move(void);

/** @brief 直接输出四轴回零角（不做插补，仅上电瞬间或自检用） */
void app_servo_set_home(void);

/** @brief 四轴是否都已到达目标（容差 0.2°） */
bool app_servo_all_at_target(void);

/** @brief 本次运动是否发生过限位钳位 */
bool app_servo_any_limited(void);

/** @brief 清除限位标志 */
void app_servo_clear_limits(void);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_APP_SERVO_H */
