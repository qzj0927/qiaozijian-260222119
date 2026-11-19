/**
 * @file    bsp_time.h
 * @brief   毫秒时基（对 HAL_GetTick 的薄封装，统一处理回绕）
 */
#ifndef BSP_BSP_TIME_H
#define BSP_BSP_TIME_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 当前毫秒计数（1ms 时基由 TIM6 中断递增 HAL 的 uwTick） */
uint32_t bsp_time_ms(void);

/** @brief 两个时刻的间隔，自动处理 32 位回绕 */
uint32_t bsp_time_elapsed(uint32_t since_ms, uint32_t now_ms);

/** @brief 是否已经超过超时时间 */
bool bsp_time_passed(uint32_t since_ms, uint32_t now_ms, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BSP_TIME_H */
