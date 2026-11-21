/**
 * @file    motion_profile.h
 * @brief   多轴同起同停的速度规划（T2）
 *
 * 思路：不为每个轴单独规划，而是规划一条归一化进度曲线 s(t) ∈ [0,1]，
 * 所有轴共用：
 *
 *     q_i(t) = q_i0 + (q_i1 - q_i0) * s(t)
 *
 * 由此天然满足 T2 的两条硬指标：
 *   1. 各轴同时出发、同时到位（同一拍 s 同时到 1）；
 *   2. 相邻两拍增量连续（s 是 C1 连续函数，增量 = Δq_i · Δs，无阶跃）。
 *
 * 限速/限加速作用在"位移最大的轴"上，其余轴按比例缩放，不会超速。
 *
 * 纯数学，无 HAL 依赖，可在 PC 上单测。
 */
#ifndef MOTION_PROFILE_H
#define MOTION_PROFILE_H

#include <stdint.h>

/** 规划形状，取值与协议 0x20 的 profile 字段一致 */
typedef enum {
    MOTION_SHAPE_STEP   = 0,   /**< 阶跃，仅调试用 */
    MOTION_SHAPE_TRAP   = 1,   /**< 梯形：加速-匀速-减速 */
    MOTION_SHAPE_SCURVE = 2    /**< S 曲线：加速度梯形，起止加速度为 0 */
} motion_shape_t;

/** 一次规划的结果 */
typedef struct {
    motion_shape_t shape;
    float duration_s;    /**< 实际总时长（请求过短会延长） */
    float ta_s;          /**< 加/减速段时长 */
    float v_peak_dps;    /**< 最大轴峰值速度 */
    float a_peak_dps2;   /**< 最大轴峰值加速度 */
} motion_plan_t;

/**
 * @brief 规划一次动作
 * @param max_delta_deg  各轴中最大角度变化量（度）
 * @param req_duration_s 请求时长（秒）；<=0 表示按限速取最短
 * @param shape          形状
 * @param v_limit_dps    最大角速度 °/s
 * @param a_limit_dps2   最大角加速度 °/s²
 */
motion_plan_t motion_plan_make(float max_delta_deg,
                               float req_duration_s,
                               motion_shape_t shape,
                               float v_limit_dps,
                               float a_limit_dps2);

/** @brief 归一化进度 0~1，t 超出 [0,T] 自动钳位 */
float motion_progress(const motion_plan_t *plan, float t_s);

/** @brief 进度对时间的一阶导（1/s），用于校验限速与连续性 */
float motion_progress_rate(const motion_plan_t *plan, float t_s);

#endif /* MOTION_PROFILE_H */
