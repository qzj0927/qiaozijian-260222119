/**
 * @file    app_main.c
 * @brief   应用入口：T0 自检 + T1 PWM + T2 多轴插补演示
 *
 * 按键用法（没有上位机时现场演示）：
 *   短按（<800ms） -> 翻转 LED（T0：1Hz 闪烁 / 常灭）
 *   长按（>800ms） -> 四轴同时走到下一个预设姿态（T2：S 曲线 800ms 同步运动）
 *
 * 200Hz 插补：主循环里用 HAL_GetTick() 做 5ms 节拍调用 app_motion_step()。
 */
#include "app_main.h"

#include "app_config.h"
#include "app_motion.h"
#include "app_servo.h"
#include "bsp_gpio.h"
#include "bsp_pwm.h"
#include "bsp_sys.h"
#include "bsp_usart.h"
#include "servo_map.h"

#include "main.h"

#include <stdio.h>

#define APP_BLINK_HALF_MS   500u    /* 500ms 翻转 = 1Hz */
#define APP_KEY_LONG_MS     800u    /* 长按阈值 */
#define APP_CTRL_PERIOD_MS    5u    /* 200Hz 插补周期 */
#define APP_POSE_MOVE_MS    800u    /* 预设姿态运动时长 */

static bool     s_blink_enabled = true;
static bool     s_led_level;
static bool     s_key_was_down;
static bool     s_key_long_fired;
static uint32_t s_key_down_ms;
static uint8_t  s_pose_idx;

/* 三个预设姿态（0.1°）：回零 / 姿态A / 姿态B */
static const int16_t k_pose[][APP_SERVO_COUNT] = {
    {0, 300, 600, 200},
    {600, -300, 400, 100},
    {-600, 500, 300, 500}
};
#define APP_POSE_COUNT (sizeof(k_pose) / sizeof(k_pose[0]))

static void print_selftest(void)
{
    static const char *const k_name[APP_SERVO_COUNT] = {"base", "shoulder", "elbow", "gripper"};

    (void)printf("\r\n");
    (void)printf("[BOOT] project=zhulkong   mcu=STM32F401CCU6\r\n");
    (void)printf("[BOOT] sysclk=%lu Hz\r\n", (unsigned long)bsp_sys_clock_hz());
    (void)printf("[BOOT] reset=%s\r\n", bsp_sys_reset_cause());
    (void)printf("[BOOT] pwm=TIM3 50Hz  range=%u~%uus  ctrl=%uHz\r\n",
                 (unsigned)APP_PWM_MIN_US, (unsigned)APP_PWM_MAX_US,
                 (unsigned)(1000u / APP_CTRL_PERIOD_MS));

    for (uint32_t i = 0; i < APP_SERVO_COUNT; ++i) {
        const servo_cfg_t *cfg = app_config_servo(i);

        (void)printf("[SELFTEST] %-8s %s  zero=%uus  us/deg=%.1f  dir=%+d\r\n",
                     k_name[i],
                     app_servo_cfg_ok(i) ? "OK  " : "FAIL",
                     (unsigned)cfg->zero_us,
                     (double)cfg->us_per_deg,
                     (int)cfg->dir);
    }
    (void)printf("[BOOT] key: short=LED  long=next pose (T2)\r\n");
}

static void next_pose(void)
{
    int16_t tgt[APP_SERVO_COUNT];

    s_pose_idx = (uint8_t)((s_pose_idx + 1u) % APP_POSE_COUNT);

    for (uint32_t i = 0; i < APP_SERVO_COUNT; ++i) {
        tgt[i] = k_pose[s_pose_idx][i];
    }

    if (app_motion_start(tgt, APP_POSE_MOVE_MS, 2u) == 0) {
        (void)printf("[T2] pose=%u  duration=%ums  profile=S-curve\r\n",
                     (unsigned)s_pose_idx, (unsigned)app_motion_duration_ms());
    } else {
        (void)printf("[T2] motion start failed\r\n");
    }
}

void app_init(void)
{
    bsp_usart_init();
    bsp_gpio_init();
    bsp_pwm_init();      /* 四路先输出 1500µs 中位 */
    app_servo_init();    /* 校验标定表并按 0° 输出 */
    app_motion_init();

    print_selftest();

    /* 上电回零：低速走到各轴 home（T3 的"上电自动回零"） */
    (void)app_motion_home();
}

void app_tick_1ms(void)
{
    /* T2 的插补在主循环里按 HAL_GetTick 驱动；
     * 这里留给 T5 的指令超时计数使用。 */
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
    uint32_t last_ctrl_ms   = HAL_GetTick();

    for (;;) {
        const uint32_t now = HAL_GetTick();

        /* ---- 200Hz 插补 ---- */
        if ((now - last_ctrl_ms) >= APP_CTRL_PERIOD_MS) {
            last_ctrl_ms = now;
            app_motion_step(now);
        }

        /* ---- LED：1Hz 闪烁 ---- */
        if ((now - last_toggle_ms) >= APP_BLINK_HALF_MS) {
            last_toggle_ms = now;
            s_led_level = (!s_led_level);

            if (s_blink_enabled) {
                bsp_led_write(s_led_level);
            }
        }

        /* ---- 按键：短按翻 LED，长按切换姿态 ---- */
        (void)bsp_key_pressed();   /* 推进消抖状态机 */
        {
            const bool down = bsp_key_is_down();

            if (down && (!s_key_was_down)) {
                s_key_down_ms    = now;
                s_key_long_fired = false;
            }

            if (down && (!s_key_long_fired) && ((now - s_key_down_ms) >= APP_KEY_LONG_MS)) {
                s_key_long_fired = true;
                next_pose();
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
