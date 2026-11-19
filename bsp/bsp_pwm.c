/**
 * @file    bsp_pwm.c
 * @brief   四路 50Hz 舵机 PWM 驱动
 *
 * 引脚（CubeMX 里已配成 TIM3 的 AF2）：
 *   CH1 PB4 底座 / CH2 PB5 肩 / CH3 PB0 肘 / CH4 PB1 夹爪
 */
#include "bsp_pwm.h"

#include "app_config.h"
#include "main.h"

extern TIM_HandleTypeDef htim3;

static const uint32_t k_channel[BSP_PWM_CHANNEL_COUNT] = {
    TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4
};

static bool s_enabled;

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
    for (uint32_t i = 0; i < BSP_PWM_CHANNEL_COUNT; ++i) {
        (void)HAL_TIM_PWM_Start(&htim3, k_channel[i]);
        __HAL_TIM_SET_COMPARE(&htim3, k_channel[i], APP_PWM_SAFE_US);
    }
    s_enabled = true;
}

void bsp_pwm_set_pulse(uint8_t ch, uint16_t us)
{
    if ((ch >= BSP_PWM_CHANNEL_COUNT) || (!s_enabled)) {
        return;
    }
    __HAL_TIM_SET_COMPARE(&htim3, k_channel[ch], clamp_pulse(us));
}

void bsp_pwm_set_all(const uint16_t us[BSP_PWM_CHANNEL_COUNT])
{
    if ((us == 0) || (!s_enabled)) {
        return;
    }
    for (uint32_t i = 0; i < BSP_PWM_CHANNEL_COUNT; ++i) {
        __HAL_TIM_SET_COMPARE(&htim3, k_channel[i], clamp_pulse(us[i]));
    }
}

void bsp_pwm_enable(bool on)
{
    for (uint32_t i = 0; i < BSP_PWM_CHANNEL_COUNT; ++i) {
        if (on) {
            (void)HAL_TIM_PWM_Start(&htim3, k_channel[i]);
        } else {
            (void)HAL_TIM_PWM_Stop(&htim3, k_channel[i]);
        }
    }
    s_enabled = on;
}

bool bsp_pwm_enabled(void)
{
    return s_enabled;
}
