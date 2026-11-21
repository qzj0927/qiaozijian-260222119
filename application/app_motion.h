/**
 * @file    app_motion.h
 * @brief   动作调度：把"目标角 + 时长 + 形状"变成 200Hz 插补输出（T2）
 *
 * 所有运动都走这一层，所以"同起同停"不是每个调用点各自保证，
 * 而是这一层的性质：四轴共用同一条归一化进度曲线。
 */
#ifndef APP_MOTION_H
#define APP_MOTION_H

#include <stdint.h>
#include <stdbool.h>

#include "app_config.h"

/** @brief 初始化（无运动） */
void app_motion_init(void);

/**
 * @brief 发起一次运动
 * @param q_tgt       四轴目标角（0.1°），内部自动按软限位钳位
 * @param duration_ms 请求时长；0 表示按限速取最短
 * @param profile     协议 0x20 的 profile：0 阶跃 / 1 梯形 / 2 S 曲线
 * @return 0 成功，-1 伺服未就绪
 */
int app_motion_start(const int16_t q_tgt[APP_SERVO_COUNT], uint16_t duration_ms, uint8_t profile);

/** @brief 四轴回零（低速梯形） */
int app_motion_home(void);

/** @brief 200Hz 调一次：按当前时刻计算进度并输出 */
void app_motion_step(uint32_t now_ms);

/** @brief 中止运动，停在当前角 */
void app_motion_abort(void);

/** @brief 是否正在运动 */
bool app_motion_busy(void);

/** @brief 本次运动的实际时长（ms） */
uint16_t app_motion_duration_ms(void);

#endif /* APP_MOTION_H */
