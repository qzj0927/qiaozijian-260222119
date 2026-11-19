/**
 * @file    bsp_sys.c
 * @brief   复位原因与时钟信息
 */
#include "bsp_sys.h"

#include "main.h"

const char *bsp_sys_reset_cause(void)
{
    const char *cause = "UNKNOWN";

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) != 0u) {
        cause = "POR";
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != 0u) {
        cause = "PIN";
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != 0u) {
        cause = "SW";
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != 0u) {
        cause = "IWDG";
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) != 0u) {
        cause = "WWDG";
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST) != 0u) {
        cause = "LPWR";
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST) != 0u) {
        cause = "BOR";
    }

    __HAL_RCC_CLEAR_RESET_FLAGS();
    return cause;
}

uint32_t bsp_sys_clock_hz(void)
{
    return HAL_RCC_GetHCLKFreq();
}

uint32_t bsp_sys_timer_clock_hz(void)
{
    /* F401 的定时器挂在 APB1/APB2 上，当 APB 预分频不为 1 时定时器时钟 x2。
     * 84MHz 配置下 APB1=42MHz、APB2=84MHz，因此 TIM2~7 = 84MHz、TIM1/9~11 = 84MHz。*/
    return HAL_RCC_GetPCLK1Freq() * 2u;
}
