/**
 * @file    bsp_pwm.c
 * @brief   四路 50Hz 舵机 PWM 输出
 *
 * 引脚（CubeMX 已配成 TIM3 的 AF2）：
 *   CH1 PA6 底座 / CH2 PA7 肩 / CH3 PB0 肘 / CH4 PB1 夹爪
 */
#include "bsp_pwm.h"

#include "main.h"

extern TIM_HandleTypeDef htim3;

static const uint32_t k_channel[APP_SERVO_COUNT] = {
    TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4
};

static uint16_t clamp_pulse(uint16_t us)
{
    if (us < APP_PWM_MIN_US) {
        return APP_PWM_MIN_US;
    }
    if (us > APP_PWM_MAX_US) {
        return APP_PWM_MAX_US;
    }
    return us;
}

void bsp_pwm_init(void)
{
    for (uint32_t i = 0; i < APP_SERVO_COUNT; ++i) {
        (void)HAL_TIM_PWM_Start(&htim3, k_channel[i]);
        __HAL_TIM_SET_COMPARE(&htim3, k_channel[i], APP_PWM_SAFE_US);
    }
}

void bsp_pwm_set_pulse(uint8_t ch, uint16_t us)
{
    if (ch >= APP_SERVO_COUNT) {
        return;
    }
    __HAL_TIM_SET_COMPARE(&htim3, k_channel[ch], clamp_pulse(us));
}

void bsp_pwm_set_all(const uint16_t us[APP_SERVO_COUNT])
{
    if (us == NULL) {
        return;
    }
    for (uint32_t i = 0; i < APP_SERVO_COUNT; ++i) {
        __HAL_TIM_SET_COMPARE(&htim3, k_channel[i], clamp_pulse(us[i]));
    }
}
