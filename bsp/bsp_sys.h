/**
 * @file    bsp_sys.h
 * @brief   系统信息：复位原因、主频（T0 自检要打印）
 */
#ifndef BSP_SYS_H
#define BSP_SYS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 读取并清除复位原因
 * @return "POR" / "PIN" / "SW" / "IWDG" / "WWDG" / "LPWR" / "BOR" / "UNKNOWN"
 * @note  读取后会清标志，只能调用一次（上电自检时）
 */
const char *bsp_sys_reset_cause(void);

/** @brief 当前系统主频（Hz） */
uint32_t bsp_sys_clock_hz(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_SYS_H */
