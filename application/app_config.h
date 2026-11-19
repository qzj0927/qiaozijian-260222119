/**
 * @file    app_config.h
 * @brief   全工程唯一的参数中心：节拍、保护门限、四轴标定表
 *
 * 任务书 T1 要求"角度到脉宽的映射参数写在配置文件或宏里，不散落在代码中"，
 * 所以所有与机械/舵机有关的数字都收敛到 app_config.c 的 g_servo_cfg[] 里，
 * 换一台机械臂只需要改这一张表 + 重新标定，业务代码一行都不用动。
 *
 * ---------------- 默认接线（STM32F401CCU6，详见 docs/02-接线表.md）----------------
 *   底座   TIM3_CH1  PB4      肩     TIM3_CH2  PB5
 *   肘     TIM3_CH3  PB0      夹爪   TIM3_CH4  PB1
 *   调试串口 USART1  PA9/PA10（115200 8N1）
 *   MPU6050  I2C1    PB8(SCL)/PB9(SDA)，400kHz
 *   USB      PA11(D-)/PA12(D+)
 *   LED      PC13（板载，低电平点亮）
 *   按键     PB12（外接，内部上拉，按下为低）
 *   1ms 时基 TIM6
 * ------------------------------------------------------------------------------
 */
#ifndef APPLICATION_APP_CONFIG_H
#define APPLICATION_APP_CONFIG_H

#include "arm_protocol.h"
#include "servo_map.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- 控制节拍 ---------------- */

#define APP_CTRL_HZ           200u    /**< 插补周期 5ms，任务书要求不低于 200Hz */
#define APP_STATUS_HZ          30u    /**< 状态上报频率，任务书要求 20~50Hz */
#define APP_SENSOR_HZ          50u    /**< MPU6050 采样频率 */
#define APP_TICK_HZ          1000u    /**< 1ms 系统时基 */

#define APP_CTRL_PERIOD_MS    (1000u / APP_CTRL_HZ)
#define APP_STATUS_PERIOD_MS  (1000u / APP_STATUS_HZ)
#define APP_SENSOR_PERIOD_MS  (1000u / APP_SENSOR_HZ)

/* ---------------- 安全与保护 ---------------- */

#define APP_CMD_TIMEOUT_MS     3000u  /**< 连续 3s 未收到 0x20 -> 保持姿态 + bit5 */
#define APP_HB_TIMEOUT_MS      3000u  /**< 连续 3s 未收到心跳 -> 同上 */
#define APP_I2C_FAULT_MS       1000u  /**< I2C 失效持续 1s -> 停止运动 + bit4/bit2 */
#define APP_I2C_RECOVER_MS      200u  /**< 连续正常读 200ms -> 自动恢复 */

/* ---------------- 运动学限制 ---------------- */

/** 单轴最大角速度（°/s）。SG90 空载约 600，带臂后取 240 稳定不丢步 */
#define APP_MAX_SPEED_DPS      240.0f
/** 单轴最大角加速度（°/s²）。越大越"冲"，越小越肉，720 是舵机可跟住的经验值 */
#define APP_MAX_ACCEL_DPS2     720.0f
/** 回零专用低速，避免上电时机械臂甩动 */
#define APP_HOME_SPEED_DPS      90.0f

/**
 * 是否把协议里的 profile=0（阶跃）强制按梯形执行。
 * 任务书 T2 明确"不允许出现阶跃"，而上位机某些交互（直接拖拽）会下发 0，
 * 所以默认按梯形走，宁可慢一点也不让关节"跳"。单轴调试时可改 0 关闭。
 */
#define APP_FORCE_SMOOTH_PROFILE 1

/* ---------------- 舵机电气限制 ---------------- */

#define APP_PWM_MIN_US          500u  /**< SG90 标称下限（实测以标定为准） */
#define APP_PWM_MAX_US         2500u  /**< SG90 标称上限 */
#define APP_PWM_PERIOD_US     20000u  /**< 50Hz */
#define APP_PWM_SAFE_US        1500u  /**< 上电/故障时的中性脉宽 */

/* ---------------- 四轴标定表 ---------------- */

/**
 * @brief 读四轴标定参数
 * @param axis 0..3，见 arm_axis_id_t
 * @return 只读指针；axis 越界返回 NULL
 */
const servo_map_cfg_t *app_config_servo(uint32_t axis);

/** @brief 轴数量与协议一致性自检（编译期） */
_Static_assert(ARM_AXIS_COUNT == 4, "本工程按四自由度机械臂固定为 4 轴");

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_APP_CONFIG_H */
