/**
 * @file    app_main.c
 * @brief   T0 板级自检
 *
 * 对应任务书 T0 判据：
 *   - 开机通过 USART 打印工程名、系统时钟频率、复位原因  -> app_init()
 *   - LED 以 1Hz 闪烁                                      -> app_loop() 每 500ms 翻转
 *   - 按键能翻转 LED 状态                                  -> bsp_key_pressed() 切换闪烁/熄灭
 */
#include "app_main.h"

#include "bsp_gpio.h"
#include "bsp_sys.h"
#include "bsp_usart.h"

#include "main.h"

#include <stdio.h>

#define APP_BLINK_HALF_MS 500u   /* 500ms 翻转一次 = 1Hz */

static bool s_blink_enabled = true;   /* 按键切换它：true=1Hz 闪烁，false=常灭 */
static bool s_led_level;              /* 当前闪烁相位 */

void app_init(void)
{
    bsp_usart_init();
    bsp_gpio_init();

    (void)printf("\r\n");
    (void)printf("[BOOT] project=zhulkong   mcu=STM32F401CCU6\r\n");
    (void)printf("[BOOT] sysclk=%lu Hz\r\n", (unsigned long)bsp_sys_clock_hz());
    (void)printf("[BOOT] reset=%s\r\n", bsp_sys_reset_cause());
    (void)printf("[BOOT] led=PC13  key=PA0  uart=115200 8N1\r\n");
    (void)printf("[BOOT] T0 self-test ready\r\n");
}

void app_tick_1ms(void)
{
    /* T0 阶段的计时直接用 HAL_GetTick()，这里先留空。
     * 等做 200Hz 插补时，会在这里置标志、在主循环里执行控制步。 */
}

/* TIM2 更新中断的回调：CubeMX 生成的 stm32f4xx_it.c 只调用 HAL_TIM_IRQHandler，
 * 具体的周期回调由 HAL 弱函数转到我们这里。 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        app_tick_1ms();
    }
}

void app_loop(void)
{
    uint32_t last_toggle_ms = HAL_GetTick();

    for (;;) {
        const uint32_t now = HAL_GetTick();

        /* 1Hz 闪烁：每 500ms 翻转一次相位 */
        if ((now - last_toggle_ms) >= APP_BLINK_HALF_MS) {
            last_toggle_ms = now;
            s_led_level = (!s_led_level);

            if (s_blink_enabled) {
                bsp_led_write(s_led_level);
            }
        }

        /* 按键：每按一次，在"1Hz 闪烁"和"常灭"之间切换 */
        if (bsp_key_pressed()) {
            s_blink_enabled = (!s_blink_enabled);
            bsp_led_write(false);   /* 先灭掉，避免按住时停在亮的状态 */
            (void)printf("[KEY] blink=%s\r\n", s_blink_enabled ? "on" : "off");
        }
    }
}
