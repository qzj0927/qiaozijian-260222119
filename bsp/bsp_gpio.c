/**
 * @file    bsp_gpio.c
 * @brief   LED 与按键驱动
 *
 * 消抖策略：每 1ms 采样一次，连续 5 次电平一致才认为稳定（5ms），
 * 这样机械抖动（<5ms）不会产生多次事件，又不会让手感变迟钝。
 */
#include "bsp_gpio.h"

#include "main.h"

#define KEY_DEBOUNCE_MS   5u
#define KEY_LONG_MS     800u

#define LED_ACTIVE_LOW    1
/* 1 = 按下为低（外接按键 + 内部上拉，默认）
 * 0 = 按下为高（多数核心板的板载 KEY 接 3V3，需同时在 CubeMX 里配下拉） */
#define KEY_ACTIVE_LOW    1

static bool     s_pressed;
static bool     s_long_sent;
static uint32_t s_press_start_ms;
static uint8_t  s_low_cnt;
static uint8_t  s_high_cnt;

void bsp_gpio_init(void)
{
    bsp_led_write(false);
}

void bsp_led_write(bool on)
{
#if (LED_ACTIVE_LOW == 1)
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
#else
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
}

void bsp_led_toggle(void)
{
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
}

bsp_key_event_t bsp_key_poll_1ms(void)
{
    GPIO_PinState raw = HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin);
#if (KEY_ACTIVE_LOW == 1)
    const bool is_low = (raw == GPIO_PIN_RESET);
#else
    const bool is_low = (raw == GPIO_PIN_SET);
#endif

    if (is_low) {
        if (s_low_cnt < KEY_DEBOUNCE_MS) {
            ++s_low_cnt;
        }
        s_high_cnt = 0u;
    } else {
        if (s_high_cnt < KEY_DEBOUNCE_MS) {
            ++s_high_cnt;
        }
        s_low_cnt = 0u;
    }

    if ((!s_pressed) && (s_low_cnt >= KEY_DEBOUNCE_MS)) {
        s_pressed        = true;
        s_long_sent      = false;
        s_press_start_ms = HAL_GetTick();
    } else if (s_pressed && (s_high_cnt >= KEY_DEBOUNCE_MS)) {
        const uint32_t held = HAL_GetTick() - s_press_start_ms;

        s_pressed = false;
        if ((held < KEY_LONG_MS) && (!s_long_sent)) {
            return BSP_KEY_SHORT;
        }
        return BSP_KEY_NONE;
    } else if (s_pressed && (!s_long_sent)
               && ((HAL_GetTick() - s_press_start_ms) >= KEY_LONG_MS)) {
        s_long_sent = true;
        return BSP_KEY_LONG;
    } else {
        /* 无事件 */
    }

    return BSP_KEY_NONE;
}
