/**
 * @file    motion_profile.h
 * @brief   多轴同起同停的速度规划（任务书 T2）
 *
 * 做法：不去为每个轴单独规划，而是规划一条"归一化进度曲线" s(t) ∈ [0,1]，
 * 让所有轴共用同一条曲线：
 *
 *      q_i(t) = q_i0 + (q_i1 - q_i0) * s(t)
 *
 * 这样天然满足 T2 的两条硬指标：
 *   1. 各轴同时出发、同时到位（同一拍 s 同时到达 1）；
 *   2. 相邻两拍的增量连续（s(t) 是 C1 连续函数，增量 = Δq_i * Δs）。
 *
 * 限速/限加速度作用在"位移最大的那个轴"上，其余轴按比例缩放，
 * 因此最大轴刚好贴着限制走，小位移轴不会超速。
 *
 * 纯数学、无 HAL 依赖，可在 PC 上单测。
 */
#ifndef COMPONENTS_MOTION_PROFILE_H
#define COMPONENTS_MOTION_PROFILE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 速度规划形状，取值与协议 0x20 载荷偏移 10 一致 */
typedef enum {
    MOTION_SHAPE_STEP   = 0,   /**< 阶跃，仅调试用，T2 不允许 */
    MOTION_SHAPE_TRAP   = 1,   /**< 梯形：加速-匀速-减速 */
    MOTION_SHAPE_SCURVE = 2    /**< S 曲线：加速度梯形，起止加速度为 0 */
} motion_shape_t;

/** 一次规划的结果 */
typedef struct {
    motion_shape_t shape;
    float duration_s;    /**< 实际总时长（请求过短时会被自动延长） */
    float ta_s;          /**< 加/减速段时长；S 曲线无匀速段时为 T/2 */
    float v_peak_dps;    /**< 最大轴的峰值速度，°/s，用于上报和校验 */
    float a_peak_dps2;   /**< 最大轴的峰值加速度，°/s² */
} motion_plan_t;

/**
 * @brief 规划一次动作
 * @param max_delta_deg 各轴中最大的角度变化量（绝对值），°；<= 0 表示不需要运动
 * @param req_duration_s 上位机请求的时长（0x20 duration_ms / 1000）；<= 0 表示按限速取最短时长
 * @param shape         规划形状
 * @param v_limit_dps   该轴允许的最大角速度，°/s
 * @param a_limit_dps2  该轴允许的最大角加速度，°/s²
 */
motion_plan_t motion_plan_make(float max_delta_deg,
                               float req_duration_s,
                               motion_shape_t shape,
                               float v_limit_dps,
                               float a_limit_dps2);

/** @brief 归一化进度 0.0 ~ 1.0；t 超出 [0, T] 时自动钳位 */
float motion_progress(const motion_plan_t *plan, float t_s);

/** @brief 归一化进度对时间的一阶导（1/s），用于校验限速与连续性 */
float motion_progress_rate(const motion_plan_t *plan, float t_s);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_MOTION_PROFILE_H */
