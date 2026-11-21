/**
 * @file    app_main.c
 * @brief   应用入口：T0 自检 + T1 单轴 PWM 演示
 *
 * T0 判据：串口打印工程名/主频/复位原因、LED 1Hz、按键翻转 LED 状态
 * T1 判据：四路 50Hz、脉宽 500~2500µs 可调、角度-脉宽线性映射
 *
 * 按键用法（没有上位机时用来现场演示）：
 *   短按（<800ms） -> 翻转 LED（T0：1Hz 闪烁 / 常灭）
 *   长按（>800ms） -> 底座角度依次跳到 -85° / -42.5° / 0° / +42.5° / +85°，
 *                    串口同时打印计算出的脉宽，可对照示波器验证映射（T1）
 */
#include "app_main.h"

#include "app_config.h"
#include "app_servo.h"
#include "bsp_gpio.h"
#include "bsp_pwm.h"
#include "bsp_sys.h"
#include "bsp_usart.h"
#include "servo_map.h"

#include "main.h"

#include <stdio.h>

#define APP_BLINK_HALF_MS 500u    /* 500ms 翻转 = 1Hz */
#define APP_KEY_LONG_MS   800u    /* 长按阈值 */

static bool     s_blink_enabled = true;
static bool     s_led_level;
static bool     s_key_was_down;
static bool     s_key_long_fired;
static uint32_t s_key_down_ms;
static uint8_t  s_demo_idx;

/* 底座演示角度（0.1°）：-85 / -42.5 / 0 / +42.5 / +85 */
static const int16_t k_demo_d10[] = {-850, -425, 0, 425, 850};

static void print_selftest(void)
{
    static const char *const k_name[APP_SERVO_COUNT] = {"base", "shoulder", "elbow", "gripper"};

    (void)printf("\r\n");
    (void)printf("[BOOT] project=zhulkong   mcu=STM32F401CCU6\r\n");
    (void)printf("[BOOT] sysclk=%lu Hz\r\n", (unsigned long)bsp_sys_clock_hz());
    (void)printf("[BOOT] reset=%s\r\n", bsp_sys_reset_cause());
    (void)printf("[BOOT] pwm=TIM3 50Hz  range=%u~%uus\r\n",
                 (unsigned)APP_PWM_MIN_US, (unsigned)APP_PWM_MAX_US);

    for (uint32_t i = 0; i < APP_SERVO_COUNT; ++i) {
        const servo_cfg_t *cfg = app_config_servo(i);

        (void)printf("[SELFTEST] %-8s %s  zero=%uus  us/deg=%.1f  dir=%+d\r\n",
                     k_name[i],
                     app_servo_cfg_ok(i) ? "OK  " : "FAIL",
                     (unsigned)cfg->zero_us,
                     (double)cfg->us_per_deg,
                     (int)cfg->dir);
    }
    (void)printf("[BOOT] key: short=LED  long=base demo\r\n");
}

static void demo_next_angle(void)
{
    const servo_cfg_t *cfg = app_config_servo(0);
    int16_t            angle;
    uint16_t           pulse;

    s_demo_idx = (uint8_t)((s_demo_idx + 1u) % (sizeof(k_demo_d10) / sizeof(k_demo_d10[0])));

    angle = app_servo_set_angle(0, k_demo_d10[s_demo_idx]);
    pulse = servo_map_angle_to_us(cfg, angle);

    (void)printf("[T1] base=%d.%d deg  pulse=%u us\r\n",
                 (int)(angle / 10), (int)((angle < 0) ? (-angle % 10) : (angle % 10)),
                 (unsigned)pulse);
}

void app_init(void)
{
    bsp_usart_init();
    bsp_gpio_init();
    bsp_pwm_init();     /* 四路先输出 1500µs 中位 */
    app_servo_init();   /* 校验标定表并按 0° 输出 */

    print_selftest();
}

void app_tick_1ms(void)
{
    /* T1 阶段还用不到 1ms 节拍；T2 插补时会在这里置标志 */
}

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

        /* ---- LED：1Hz 闪烁 ---- */
        if ((now - last_toggle_ms) >= APP_BLINK_HALF_MS) {
            last_toggle_ms = now;
            s_led_level = (!s_led_level);

            if (s_blink_enabled) {
                bsp_led_write(s_led_level);
            }
        }

        /* ---- 按键：短按翻 LED，长按切底座角度 ---- */
        (void)bsp_key_pressed();   /* 推进消抖状态机 */
        {
            const bool down = bsp_key_is_down();

            if (down && (!s_key_was_down)) {
                s_key_down_ms    = now;
                s_key_long_fired = false;
            }

            if (down && (!s_key_long_fired) && ((now - s_key_down_ms) >= APP_KEY_LONG_MS)) {
                s_key_long_fired = true;
                demo_next_angle();
            }

            if ((!down) && s_key_was_down && (!s_key_long_fired)) {
                s_blink_enabled = (!s_blink_enabled);
                bsp_led_write(false);
                (void)printf("[T0] blink=%s\r\n", s_blink_enabled ? "on" : "off");
            }

            s_key_was_down = down;
        }
    }
}
