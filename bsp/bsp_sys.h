/**
 * @file    bsp_sys.h
 * @brief   系统信息：复位原因、时钟频率（T0 自检打印用）
 */
#ifndef BSP_BSP_SYS_H
#define BSP_BSP_SYS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 读取并清除复位原因
 * @return 静态字符串："POR"/"PIN"/"SW"/"IWDG"/"WWDG"/"LPWR"/"BOR"/"UNKNOWN"
 * @note   必须在上电自检时调用一次，读完立刻清标志，否则下次读到的还是旧值
 */
const char *bsp_sys_reset_cause(void);

/** @brief 系统主频 Hz */
uint32_t bsp_sys_clock_hz(void);

/** @brief 定时器时钟 Hz（APB1/APB2 定时器倍频后的频率） */
uint32_t bsp_sys_timer_clock_hz(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BSP_SYS_H */
