/**
 * @file    bsp_gpio.h
 * @brief   LED 与按键（T0：LED 1Hz 闪烁、按键翻转 LED 状态）
 *
 * 硬件：LED = PC13（板载，低电平点亮）
 *       按键 = PA0（板载 KEY，按下接 3V3，高电平；CubeMX 里已配下拉）
 */
#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 初始化：LED 熄灭，按键消抖状态复位 */
void bsp_gpio_init(void);

/** @brief LED 开关（true = 点亮），内部处理低电平有效 */
void bsp_led_write(bool on);

/** @brief LED 翻转 */
void bsp_led_toggle(void);

/**
 * @brief 查询"按键刚被按下"的事件（20ms 消抖，只在按下瞬间返回一次 true）
 * @note  在主循环里反复调用即可，不能放在中断里
 */
bool bsp_key_pressed(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_GPIO_H */
