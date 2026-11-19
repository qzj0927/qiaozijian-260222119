/**
 * @file    mpu6050.h
 * @brief   MPU6050 姿态传感器驱动（T4）
 *
 * 关键点：
 *  1. 硬件 I2C 400kHz，一次读 14 字节（加速度 6 + 温度 2 + 角速度 6），
 *     保证加速度和角速度来自同一采样时刻；
 *  2. 静止零偏校准：上电时采 N 个样本，陀螺零偏与俯仰零位一次算完；
 *  3. 俯仰角用加速度算（静态精度足够达到 T4 的 3° 判据），
 *     再用一阶低通抑制舵机抖动带来的高频噪声；
 *  4. 任何一次读失败都立刻返回 false，由上层累计时长判定故障。
 */
#ifndef COMPONENTS_MPU6050_H
#define COMPONENTS_MPU6050_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 传感器的一次采样结果 */
typedef struct {
    float pitch_deg;      /**< 滤波后俯仰角，°（已减零位） */
    float accel_g[3];     /**< 加速度，g */
    float rate_dps[3];    /**< 角速度，°/s（已减零偏） */
    bool  valid;          /**< 本次数据是否可信 */
} mpu6050_sample_t;

/**
 * @brief 初始化：复位、设量程 ±2g / ±250dps、采样率 200Hz、DLPF 44Hz
 * @return true 表示 WHO_AM_I 读到 0x68
 */
bool mpu6050_init(void);

/**
 * @brief 静止零偏校准
 * @param samples    样本数（建议 200，约 1s）
 * @param period_ms  采样间隔（建议 5ms）
 * @return true 表示校准期间所有样本都读到
 * @note  校准期间机械臂必须静止，否则零位会偏，T4 的 3° 判据直接不达标
 */
bool mpu6050_calibrate(uint16_t samples, uint16_t period_ms);

/**
 * @brief 读一次并更新滤波结果
 * @param out 输出；读失败时 out->valid = false
 * @return true 表示本次 I2C 读写成功
 */
bool mpu6050_poll(mpu6050_sample_t *out);

/** @brief 把当前俯仰角置为零位（现场校准用，替代重新上电） */
void mpu6050_set_zero_here(void);

/** @brief 复位低通滤波器（大角度跳变后调用，避免"拖着走"） */
void mpu6050_reset_filter(void);

/** @brief 上次 WHO_AM_I 的值，自检打印用 */
uint8_t mpu6050_who_am_i(void);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_MPU6050_H */
