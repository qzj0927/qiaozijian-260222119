/**
 * @file    bsp_gpio.c
 * @brief   LED 与按键驱动
 *
 * CubeMX 的 User Label 填的是小写 led / key，生成宏为 led_Pin / key_Pin。
 * 这里统一成 APP_LED_xxx 别名，后面换引脚只改这三行，业务代码不受影响。
 */
#include "bsp_gpio.h"

#include "main.h"

#define APP_LED_PORT   led_GPIO_Port
#define APP_LED_PIN    led_Pin
#define APP_KEY_PORT   key_GPIO_Port
#define APP_KEY_PIN    key_Pin

#define LED_ACTIVE_LOW   1   /* PC13 板载 LED：输出低电平才亮 */
#define KEY_ACTIVE_HIGH  1   /* 板载 KEY 按下接 3V3；若你的板子接 GND，改成 0 并配上拉 */
#define KEY_DEBOUNCE_MS  20u

static bool     s_key_level_last;   /* 上一次采样到的原始电平（已按有效电平归一） */
static bool     s_key_stable;       /* 消抖后的稳定状态 */
static uint32_t s_key_change_ms;    /* 原始电平最后一次变化的时刻 */

void bsp_gpio_init(void)
{
    bsp_led_write(false);

    s_key_level_last = false;
    s_key_stable     = false;
    s_key_change_ms  = 0u;
}

void bsp_led_write(bool on)
{
#if (LED_ACTIVE_LOW == 1)
    HAL_GPIO_WritePin(APP_LED_PORT, APP_LED_PIN, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
#else
    HAL_GPIO_WritePin(APP_LED_PORT, APP_LED_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
}

void bsp_led_toggle(void)
{
    HAL_GPIO_TogglePin(APP_LED_PORT, APP_LED_PIN);
}

bool bsp_key_pressed(void)
{
    GPIO_PinState raw_state = HAL_GPIO_ReadPin(APP_KEY_PORT, APP_KEY_PIN);
#if (KEY_ACTIVE_HIGH == 1)
    const bool pressed = (raw_state == GPIO_PIN_SET);
#else
    const bool pressed = (raw_state == GPIO_PIN_RESET);
#endif

    const uint32_t now = HAL_GetTick();

    /* 电平变化：重新计时，先不下结论（抖动期） */
    if (pressed != s_key_level_last) {
        s_key_level_last = pressed;
        s_key_change_ms  = now;
        return false;
    }

    /* 电平稳定不足 20ms：继续等 */
    if ((now - s_key_change_ms) < KEY_DEBOUNCE_MS) {
        return false;
    }

    /* 稳定在"按下"，且之前记录的是"松开"：产生一次按键事件 */
    if (pressed && (!s_key_stable)) {
        s_key_stable = true;
        return true;
    }

    if (!pressed) {
        s_key_stable = false;
    }
    return false;
}
