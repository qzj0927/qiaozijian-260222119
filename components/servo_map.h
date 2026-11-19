/**
 * @file    servo_map.h
 * @brief   关节角 ↔ 舵机脉宽的映射与限位钳位（任务书 T1 / T3）
 *
 * 设计原则：
 *  - 映射参数只存在配置表里（application/app_config.h），代码里不出现裸数字；
 *  - 映射函数自己负责钳位，调用者不可能输出超出标定行程的脉宽；
 *  - 纯计算、无 HAL 依赖，可在 PC 上单测。
 */
#ifndef COMPONENTS_SERVO_MAP_H
#define COMPONENTS_SERVO_MAP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 单轴标定参数：一次标定、全局复用 */
typedef struct {
    uint16_t pulse_min_us;    /**< 实测可用下限，如 500（不得低于舵机标称下限） */
    uint16_t pulse_max_us;    /**< 实测可用上限，如 2400 */
    uint16_t zero_us;         /**< 关节角 0.0° 对应的脉宽 */
    float    us_per_deg;      /**< 每 1° 对应的脉宽增量（SG90 约 10~11） */
    int8_t   dir;             /**< 装配方向 +1 / -1 */
    int16_t  angle_min_d10;   /**< 软限位下限，单位 0.1° */
    int16_t  angle_max_d10;   /**< 软限位上限，单位 0.1° */
    int16_t  home_d10;        /**< 回零位置，单位 0.1° */
} servo_map_cfg_t;

/** @brief 把角度钳到软限位内 */
int16_t servo_map_clamp_angle(const servo_map_cfg_t *cfg, int16_t q_d10);

/** @brief 角度是否在软限位内（不修改入参） */
bool servo_map_angle_ok(const servo_map_cfg_t *cfg, int16_t q_d10);

/**
 * @brief 角度 -> 脉宽（微秒）
 * @note  先按软限位钳位，再按 [pulse_min_us, pulse_max_us] 钳位，
 *        因此返回值永远落在标定行程内。
 */
uint16_t servo_map_angle_to_us(const servo_map_cfg_t *cfg, int16_t q_d10);

/** @brief 脉宽 -> 角度（0.1°），用于自检打印和标定反算 */
int16_t servo_map_us_to_angle(const servo_map_cfg_t *cfg, uint16_t us);

/**
 * @brief 配置自检：限位是否闭合、零位是否在行程内、限位端点是否落在脉宽范围内
 * @return true 表示配置可用；上电时调用一次，失败即打印并停在故障态
 */
bool servo_map_cfg_sane(const servo_map_cfg_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_SERVO_MAP_H */
