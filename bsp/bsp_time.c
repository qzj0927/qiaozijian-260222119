/**
 * @file    bsp_time.c
 * @brief   毫秒时基
 *
 * 时基来自 CubeMX 配置的 TIM6（84MHz/84 -> 1MHz，ARR=999 -> 1ms 更新中断），
 * 中断里调用 HAL_IncTick()，因此 HAL_GetTick() 就是 1ms 计数。
 * 任务书 T0 的"至少 1 个 1ms 定时器作为控制节拍"由它满足。
 */
#include "bsp_time.h"

#include "main.h"

uint32_t bsp_time_ms(void)
{
    return HAL_GetTick();
}

uint32_t bsp_time_elapsed(uint32_t since_ms, uint32_t now_ms)
{
    /* 无符号减法天然处理回绕：间隔只要小于 2^32 ms（约 49 天）都正确 */
    return now_ms - since_ms;
}

bool bsp_time_passed(uint32_t since_ms, uint32_t now_ms, uint32_t timeout_ms)
{
    return bsp_time_elapsed(since_ms, now_ms) >= timeout_ms;
}
