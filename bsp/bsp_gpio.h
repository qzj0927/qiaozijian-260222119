/**
 * @file    bsp_gpio.h
 * @brief   LED 与按键（T0）
 *
 * 硬件：LED = PC13（板载，低电平点亮）
 *       按键 = PA0（板载 KEY，按下接 3V3，高电平；CubeMX 已配下拉）
 */
#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 初始化：LED 熄灭，按键状态复位 */
void bsp_gpio_init(void);

/** @brief LED 开关（true = 点亮） */
void bsp_led_write(bool on);

/** @brief LED 翻转 */
void bsp_led_toggle(void);

/** @brief 查询"刚按下"事件（20ms 消抖，按下瞬间返回一次 true） */
bool bsp_key_pressed(void);

/** @brief 按键当前是否处于按下状态（已消抖），供长短按判定使用 */
bool bsp_key_is_down(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_GPIO_H */
