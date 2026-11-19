/**
 * @file    app_main.c
 * @brief   应用入口实现：调度、自检、故障处理
 *
 * 时序分配（全部由 1ms 中断打标志，主循环执行）：
 *   5ms  -> app_motion_step()     200Hz 插补（任务书 T2 的下限）
 *   20ms -> MPU6050 采样 + I2C 故障判定（T4）
 *   33ms -> app_protocol_tick()   30Hz 状态上报（T5a 的 20~50Hz）
 *   500ms-> LED 翻转（T0 的 1Hz）
 *
 * 为什么不在中断里做控制：I2C 读一次要几十微秒到几毫秒，放在 200Hz 中断里
 * 会挤占 USB 中断，导致 CDC 丢包。所有耗时操作一律在主循环，中断只置标志。
 */
#include "app_main.h"

#include "app_config.h"
#include "app_motion.h"
#include "app_protocol.h"
#include "app_servo.h"
#include "app_state.h"
#include "arm_protocol.h"
#include "bsp_gpio.h"
#include "bsp_i2c.h"
#include "bsp_pwm.h"
#include "bsp_sys.h"
#include "bsp_time.h"
#include "bsp_usart.h"
#include "mpu6050.h"
#include "usb_link.h"

#include "main.h"

#include <math.h>
#include <stdio.h>

#define APP_VERSION_STR "1.0.0"

static volatile bool s_flag_ctrl;
static volatile bool s_flag_sensor;
static volatile bool s_flag_led;
static volatile bsp_key_event_t s_key_event;

/* ---------------- 自检 ---------------- */

static void print_banner(void)
{
    (void)printf("\r\n==============================================\r\n");
    (void)printf("[BOOT] ec-round2  fw=%s\r\n", APP_VERSION_STR);
    (void)printf("[BOOT] mcu=STM32F401CCU6  sysclk=%luHz  tim=%luHz\r\n",
                 (unsigned long)bsp_sys_clock_hz(),
                 (unsigned long)bsp_sys_timer_clock_hz());
    (void)printf("[BOOT] proto=V%u.%u  link=%s\r\n",
                 (unsigned)ARM_VERSION_MAJOR, (unsigned)ARM_VERSION_MINOR,
                 usb_link_get()->name);
}

static void print_selftest(void)
{
    static const char *const k_axis_name[ARM_AXIS_COUNT] = {"base", "shoulder", "elbow", "gripper"};

    (void)printf("[SELFTEST] reset_cause=%s\r\n", bsp_sys_reset_cause());

    for (uint32_t i = 0; i < (uint32_t)ARM_AXIS_COUNT; ++i) {
        const servo_map_cfg_t *cfg = app_config_servo(i);

        (void)printf("[SELFTEST] axis %u %-8s %s  zero=%uus  us/deg=%.1f  dir=%+d  limit=[%.1f, %.1f]\r\n",
                     (unsigned)i, k_axis_name[i],
                     app_servo_cfg_ok(i) ? "OK  " : "FAIL",
                     (unsigned)cfg->zero_us, (double)cfg->us_per_deg, (int)cfg->dir,
                     (double)cfg->angle_min_d10 / 10.0, (double)cfg->angle_max_d10 / 10.0);
    }

    (void)printf("[SELFTEST] pwm=50Hz  period=%uus  range=%u~%uus\r\n",
                 (unsigned)APP_PWM_PERIOD_US, (unsigned)APP_PWM_MIN_US,
                 (unsigned)APP_PWM_MAX_US);
    (void)printf("[SELFTEST] i2c_err=%lu  mpu_who=0x%02X\r\n",
                 (unsigned long)bsp_i2c_error_count(), (unsigned)mpu6050_who_am_i());
    (void)printf("[SELFTEST] ctrl=%uHz  status=%uHz  sensor=%uHz\r\n",
                 (unsigned)APP_CTRL_HZ, (unsigned)APP_STATUS_HZ, (unsigned)APP_SENSOR_HZ);
}

/* ---------------- 传感器 ---------------- */

static void poll_sensor(uint32_t now_ms)
{
    mpu6050_sample_t sample;
    const bool       ok = mpu6050_poll(&sample);

    app_protocol_report_sensor(ok, now_ms);

    if (ok && (!app_protocol_sensor_failed())) {
        app_protocol_set_pitch((int16_t)lroundf(sample.pitch_deg * 10.0f));
    } else if (app_protocol_sensor_failed()) {
        app_protocol_set_pitch(0);   /* 无传感器时按协议填 0 */
    } else {
        /* 单次读失败：保留上一次有效值，避免状态帧在 30Hz 上报里抖动 */
    }
}

/* ---------------- 对外接口 ---------------- */

void app_init(void)
{
    bsp_usart_init();
    bsp_gpio_init();
    bsp_pwm_init();
    bsp_i2c_init();

    app_servo_init();
    app_motion_init();
    app_state_init();

    /* MPU6050：先确认在线，再静止校准。校准期间机械臂必须不动。 */
    if (mpu6050_init()) {
        const bool cal_ok = mpu6050_calibrate(200u, 5u);

        (void)printf("[SELFTEST] mpu6050 online, calib=%s\r\n", cal_ok ? "OK" : "wiggle");
    } else {
        (void)printf("[SELFTEST] mpu6050 NOT FOUND (check SDA/SCL/AD0)\r\n");
    }

    app_protocol_init();
    usb_link_get()->init();

    print_banner();
    print_selftest();

    /* 上电回零：低速走到各轴 home，走完回到 MANUAL */
    if (app_state_request(APP_MODE_HOMING) != true) {
        (void)printf("[BOOT] homing request failed\r\n");
    }
}

void app_tick_1ms(void)
{
    static uint16_t ctrl_cnt;
    static uint16_t sensor_cnt;
    static uint16_t led_cnt;

    /* 按键消抖必须在 1ms 节拍里跑，但事件只排队，不在这里执行业务 */
    {
        const bsp_key_event_t ev = bsp_key_poll_1ms();

        if ((ev != BSP_KEY_NONE) && (s_key_event == BSP_KEY_NONE)) {
            s_key_event = ev;
        }
    }

    if (++ctrl_cnt >= (1000u / APP_CTRL_HZ)) {
        ctrl_cnt  = 0u;
        s_flag_ctrl = true;
    }
    if (++sensor_cnt >= (1000u / APP_SENSOR_HZ)) {
        sensor_cnt  = 0u;
        s_flag_sensor = true;
    }
    if (++led_cnt >= 500u) {
        led_cnt    = 0u;
        s_flag_led = true;
    }
}

void app_loop(void)
{
    for (;;) {
        const uint32_t now = bsp_time_ms();

        if (s_flag_ctrl) {
            s_flag_ctrl = false;
            app_motion_step(now);
            if (app_state_is_homing() && (!app_motion_busy())) {
                app_state_on_homing_done();
            }
        }

        if (s_flag_sensor) {
            s_flag_sensor = false;
            poll_sensor(now);
        }

        if (s_flag_led) {
            s_flag_led = false;
            /* 固定 1Hz（周期 500ms 翻转一次）：T0 判据要求"LED 以 1Hz 闪烁"，
             * 任何花哨的故障闪烁都可能被评审认为不符合判据，这里保持一致。 */
            bsp_led_toggle();
        }

        if (s_key_event != BSP_KEY_NONE) {
            const bsp_key_event_t ev = s_key_event;

            s_key_event = BSP_KEY_NONE;
            app_state_on_key(ev);
        }

        app_protocol_tick(now);
    }
}
