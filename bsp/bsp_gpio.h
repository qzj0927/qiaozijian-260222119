/**
 * @file    bsp_gpio.h
 * @brief   LED 与按键（T0 的"LED 1Hz 闪烁、按键翻转状态"）
 *
 * 默认接线：LED = PC13（板载，低电平点亮）、KEY = PB12（外接，内部上拉，按下为低）。
 *
 * 想用板载按键（多数 F401CCU6 核心板的 KEY 在 PA0，按下接 3V3，高有效）：
 *   1. CubeMX 里把该引脚设为 GPIO_Input，Pull-down，User Label 填 KEY；
 *   2. 把 bsp_gpio.c 顶部的 KEY_ACTIVE_LOW 改成 0；
 *   其余代码一行都不用动（判定逻辑只认"有效电平"，不认具体引脚）。
 */
#ifndef BSP_BSP_GPIO_H
#define BSP_BSP_GPIO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 按键事件：短按 / 长按由 bsp_key_poll_1ms 内部判定 */
typedef enum {
    BSP_KEY_NONE = 0,
    BSP_KEY_SHORT,   /**< 按下并释放，按住在 50ms~800ms 之间 */
    BSP_KEY_LONG     /**< 持续按住超过 800ms，只上报一次 */
} bsp_key_event_t;

/** @brief 初始化（上电 LED 熄灭，按键引脚已是上拉输入） */
void bsp_gpio_init(void);

/** @brief LED 开关，自动处理低电平有效 */
void bsp_led_write(bool on);

/** @brief LED 翻转（1Hz 闪烁就是每 500ms 调一次） */
void bsp_led_toggle(void);

/**
 * @brief 每 1ms 调用一次，做 20ms 消抖与长短按判定
 * @return 事件，无事件返回 BSP_KEY_NONE
 */
bsp_key_event_t bsp_key_poll_1ms(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BSP_GPIO_H */
