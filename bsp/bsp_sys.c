/**
 * @file    bsp_sys.c
 * @brief   复位原因与主频
 *
 * T0 判据要求串口打印"复位原因"，评审会按复位键验证是否变成 PIN。
 */
#include "bsp_sys.h"

#include "main.h"

const char *bsp_sys_reset_cause(void)
{
    const char *cause = "UNKNOWN";

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) != 0u) {
        cause = "POR";          /* 上电复位 */
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != 0u) {
        cause = "PIN";          /* 按复位键 / NRST 被拉低 */
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != 0u) {
        cause = "SW";           /* 软件复位（NVIC_SystemReset） */
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != 0u) {
        cause = "IWDG";
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) != 0u) {
        cause = "WWDG";
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST) != 0u) {
        cause = "LPWR";
    } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST) != 0u) {
        cause = "BOR";
    } else {
        /* 保持 UNKNOWN */
    }

    /* 只读一次：不清标志的话，下次上电还是读到这里 */
    __HAL_RCC_CLEAR_RESET_FLAGS();
    return cause;
}

uint32_t bsp_sys_clock_hz(void)
{
    return HAL_RCC_GetHCLKFreq();
}
