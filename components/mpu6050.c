/**
 * @file    mpu6050.c
 * @brief   MPU6050 驱动：初始化、零偏校准、俯仰角解算
 *
 * 装配约定（写进接线表）：模块 X 轴指向小臂末端，Z 轴垂直小臂表面向上，
 * 绕 Y 轴俯仰。若实际装配不同，只需改 MPU6050_PITCH_SIGN 与安装方向，
 * 不必改算法。
 */
#include "mpu6050.h"

#include "bsp_i2c.h"
#include "bsp_time.h"

#include "main.h"

#include <math.h>

#define MPU6050_ADDR            0x68u
#define MPU6050_REG_SMPLRT_DIV  0x19u
#define MPU6050_REG_CONFIG      0x1Au
#define MPU6050_REG_GYRO_CFG    0x1Bu
#define MPU6050_REG_ACCEL_CFG   0x1Cu
#define MPU6050_REG_ACCEL_XOUT  0x3Bu
#define MPU6050_REG_PWR_MGMT_1  0x6Bu
#define MPU6050_REG_WHO_AM_I    0x75u

#define MPU6050_WHO_AM_I_VAL    0x68u
#define MPU6050_ACCEL_LSB_PER_G 16384.0f   /* ±2g */
#define MPU6050_GYRO_LSB_PER_DPS 131.0f    /* ±250dps */
#define MPU6050_RAD_TO_DEG      57.2957795f

/** 装配方向：抬手时俯仰角应变大。若反过来，改成 -1.0f */
#define MPU6050_PITCH_SIGN      (+1.0f)
/** 一阶低通系数：越小越平滑但越滞后。0.25 ≈ 截止 8Hz @50Hz 采样 */
#define MPU6050_IIR_ALPHA       0.25f

#define MPU6050_I2C_TIMEOUT_MS  10u

static struct {
    int16_t  gyro_bias[3];      /* 原始 LSB */
    float    pitch_zero_deg;
    float    pitch_filt_deg;
    bool     calibrated;
    bool     filter_init;
    uint8_t  who_am_i;
} s_mpu;

static int16_t be16(const uint8_t *p)
{
    return (int16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

bool mpu6050_init(void)
{
    uint8_t value = 0u;
    uint8_t who = 0u;

    s_mpu.calibrated   = false;
    s_mpu.filter_init  = false;
    s_mpu.pitch_filt_deg = 0.0f;
    s_mpu.pitch_zero_deg = 0.0f;
    for (uint32_t i = 0u; i < 3u; ++i) {
        s_mpu.gyro_bias[i] = 0;
    }

    if (bsp_i2c_read(MPU6050_ADDR, MPU6050_REG_WHO_AM_I, &who, 1u, MPU6050_I2C_TIMEOUT_MS)
        != BSP_I2C_OK) {
        s_mpu.who_am_i = 0u;
        return false;
    }
    s_mpu.who_am_i = who;
    if (who != MPU6050_WHO_AM_I_VAL) {
        return false;
    }

    /* 1. 退出睡眠，时钟源选 PLL（陀螺 X 轴），比内部 RC 稳定得多 */
    value = 0x01u;
    if (bsp_i2c_write(MPU6050_ADDR, MPU6050_REG_PWR_MGMT_1, &value, 1u, MPU6050_I2C_TIMEOUT_MS)
        != BSP_I2C_OK) {
        return false;
    }
    HAL_Delay(50u);

    /* 2. 采样率：1kHz / (1 + 4) = 200Hz */
    value = 0x04u;
    if (bsp_i2c_write(MPU6050_ADDR, MPU6050_REG_SMPLRT_DIV, &value, 1u, MPU6050_I2C_TIMEOUT_MS)
        != BSP_I2C_OK) {
        return false;
    }

    /* 3. DLPF 44Hz：滤掉舵机带来的高频抖动 */
    value = 0x03u;
    if (bsp_i2c_write(MPU6050_ADDR, MPU6050_REG_CONFIG, &value, 1u, MPU6050_I2C_TIMEOUT_MS)
        != BSP_I2C_OK) {
        return false;
    }

    /* 4. 量程 ±250dps / ±2g */
    value = 0x00u;
    if (bsp_i2c_write(MPU6050_ADDR, MPU6050_REG_GYRO_CFG, &value, 1u, MPU6050_I2C_TIMEOUT_MS)
        != BSP_I2C_OK) {
        return false;
    }
    if (bsp_i2c_write(MPU6050_ADDR, MPU6050_REG_ACCEL_CFG, &value, 1u, MPU6050_I2C_TIMEOUT_MS)
        != BSP_I2C_OK) {
        return false;
    }

    return true;
}

bool mpu6050_calibrate(uint16_t samples, uint16_t period_ms)
{
    uint8_t buf[14];
    int32_t gyro_sum[3] = {0, 0, 0};
    float   pitch_sum = 0.0f;
    float   pitch_min = 1e9f;
    float   pitch_max = -1e9f;
    uint16_t ok = 0u;

    if (samples == 0u) {
        return false;
    }

    for (uint16_t i = 0u; i < samples; ++i) {
        if (bsp_i2c_read(MPU6050_ADDR, MPU6050_REG_ACCEL_XOUT, buf, sizeof(buf),
                         MPU6050_I2C_TIMEOUT_MS) == BSP_I2C_OK) {
            const float ax = (float)be16(&buf[0]) / MPU6050_ACCEL_LSB_PER_G;
            const float ay = (float)be16(&buf[2]) / MPU6050_ACCEL_LSB_PER_G;
            const float az = (float)be16(&buf[4]) / MPU6050_ACCEL_LSB_PER_G;
            const float pitch = MPU6050_PITCH_SIGN
                                * atan2f(ax, sqrtf((ay * ay) + (az * az))) * MPU6050_RAD_TO_DEG;

            gyro_sum[0] += be16(&buf[8]);
            gyro_sum[1] += be16(&buf[10]);
            gyro_sum[2] += be16(&buf[12]);
            pitch_sum += pitch;
            if (pitch < pitch_min) {
                pitch_min = pitch;
            }
            if (pitch > pitch_max) {
                pitch_max = pitch;
            }
            ++ok;
        }
        HAL_Delay(period_ms);
    }

    if (ok < (samples / 2u)) {
        return false;
    }

    for (uint32_t i = 0u; i < 3u; ++i) {
        s_mpu.gyro_bias[i] = (int16_t)(gyro_sum[i] / (int32_t)ok);
    }
    s_mpu.pitch_zero_deg = pitch_sum / (float)ok;
    s_mpu.pitch_filt_deg = 0.0f;
    s_mpu.filter_init    = true;
    s_mpu.calibrated     = true;

    /* 校准期间抖动大于 2° 说明有人在碰机械臂，返回值仍给 true 但要提醒 */
    return (pitch_max - pitch_min) <= 2.0f;
}

bool mpu6050_poll(mpu6050_sample_t *out)
{
    uint8_t buf[14];

    if (out == 0) {
        return false;
    }

    out->valid = false;
    if (bsp_i2c_read(MPU6050_ADDR, MPU6050_REG_ACCEL_XOUT, buf, sizeof(buf),
                     MPU6050_I2C_TIMEOUT_MS) != BSP_I2C_OK) {
        return false;
    }

    out->accel_g[0] = (float)be16(&buf[0]) / MPU6050_ACCEL_LSB_PER_G;
    out->accel_g[1] = (float)be16(&buf[2]) / MPU6050_ACCEL_LSB_PER_G;
    out->accel_g[2] = (float)be16(&buf[4]) / MPU6050_ACCEL_LSB_PER_G;

    for (uint32_t i = 0u; i < 3u; ++i) {
        out->rate_dps[i] = ((float)be16(&buf[8u + (i * 2u)]) - (float)s_mpu.gyro_bias[i])
                           / MPU6050_GYRO_LSB_PER_DPS;
    }

    {
        const float ax = out->accel_g[0];
        const float ay = out->accel_g[1];
        const float az = out->accel_g[2];
        const float pitch = (MPU6050_PITCH_SIGN
                             * atan2f(ax, sqrtf((ay * ay) + (az * az))) * MPU6050_RAD_TO_DEG)
                            - s_mpu.pitch_zero_deg;

        if (!s_mpu.filter_init) {
            s_mpu.pitch_filt_deg = pitch;
            s_mpu.filter_init    = true;
        } else {
            s_mpu.pitch_filt_deg += MPU6050_IIR_ALPHA * (pitch - s_mpu.pitch_filt_deg);
        }
        out->pitch_deg = s_mpu.pitch_filt_deg;
    }

    out->valid = true;
    return true;
}

void mpu6050_set_zero_here(void)
{
    s_mpu.pitch_zero_deg = s_mpu.pitch_filt_deg + s_mpu.pitch_zero_deg;
    s_mpu.pitch_filt_deg = 0.0f;
}

void mpu6050_reset_filter(void)
{
    s_mpu.filter_init = false;
}

uint8_t mpu6050_who_am_i(void)
{
    return s_mpu.who_am_i;
}
