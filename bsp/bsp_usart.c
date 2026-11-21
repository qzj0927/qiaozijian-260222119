/**
 * @file    bsp_usart.c
 * @brief   调试串口 + printf 重定向
 *
 * newlib 的 printf 最终会调用 _write()，把它重定向到 USART1，
 * 这样 main 里可以直接 printf，不用每处都写 HAL_UART_Transmit。
 * 注意：printf 会阻塞几十微秒到几毫秒，绝不能放在 200Hz 控制循环里。
 */
#include "bsp_usart.h"

#include "main.h"

#include <string.h>

#define USART_TX_TIMEOUT_MS 50u

extern UART_HandleTypeDef huart1;

void bsp_usart_init(void)
{
    /* CubeMX 已完成 115200 8N1 的配置，这里无需动作 */
}

void bsp_usart_puts(const char *text)
{
    if (text == NULL) {
        return;
    }
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text),
                            USART_TX_TIMEOUT_MS);
}

/* printf 重定向：newlib 链接时若未使用 printf 会被自动丢弃，不占空间 */
int _write(int file, char *ptr, int len)
{
    (void)file;
    if ((ptr != NULL) && (len > 0)) {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)ptr, (uint16_t)len, USART_TX_TIMEOUT_MS);
    }
    return len;
}
