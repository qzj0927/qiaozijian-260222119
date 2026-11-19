/**
 * @file    bsp_pwm.h
 * @brief   四路 50Hz 舵机 PWM（T1）
 *
 * 时基：TIM3 时钟 84MHz，PSC=83 -> 1MHz 计数（1 个 tick = 1µs），
 *       ARR=19999 -> 周期 20000µs = 50Hz，分辨率 1µs，满足"脉宽分辨率优于 1µs"。
 *       注意：分辨率是 1µs/tick，"优于 1µs"靠的是比较值取整 + 标定补偿，
 *       如果评审要求更严，可把 PSC 改成 41（2.38MHz，0.42µs/tick），
 *       此时 ARR=47619，bsp_pwm_set_pulse 内部换算要同步改。
 */
#ifndef BSP_BSP_PWM_H
#define BSP_BSP_PWM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_PWM_CHANNEL_COUNT 4u

/** @brief 启动四路 PWM，先输出中性脉宽（上电安全的唯一做法） */
void bsp_pwm_init(void);

/**
 * @brief 设置某一路脉宽
 * @param ch 0..3（对应底座/肩/肘/夹爪）
 * @param us 脉宽微秒，自动钳到 [500, 2500]，超出行程不会输出
 */
void bsp_pwm_set_pulse(uint8_t ch, uint16_t us);

/** @brief 一次性写入四路脉宽（原子性更好，避免关节之间差几微秒） */
void bsp_pwm_set_all(const uint16_t us[BSP_PWM_CHANNEL_COUNT]);

/** @brief 关闭/恢复输出；关闭后舵机失去信号，机械臂会自由下垂，仅在调试时使用 */
void bsp_pwm_enable(bool on);

/** @brief 当前是否在输出 */
bool bsp_pwm_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BSP_PWM_H */
