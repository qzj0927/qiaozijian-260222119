/**
 * @file    bsp_usart.c
 * @brief   调试串口实现 + printf 重定向
 *
 * 重定向走 _write（newlib），因此 printf 的浮点支持受链接选项影响：
 * 需要打印 float 时在链接选项里加 -u _printf_float。
 * 自检信息尽量用整数打印，省 4KB flash。
 */
#include "bsp_usart.h"

#include "main.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define USART_TX_TIMEOUT_MS 50u

extern UART_HandleTypeDef huart1;

void bsp_usart_init(void)
{
    /* CubeMX 已配置 115200 8N1；这里不做任何事，保留接口方便以后加 DMA */
}

void bsp_usart_puts(const char *text)
{
    if (text == 0) {
        return;
    }
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), USART_TX_TIMEOUT_MS);
}

void bsp_usart_printf(const char *fmt, ...)
{
    char    buf[160];
    va_list args;
    int     n;

    if (fmt == 0) {
        return;
    }

    va_start(args, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (n <= 0) {
        return;
    }
    if ((uint32_t)n >= sizeof(buf)) {
        n = (int)sizeof(buf) - 1;
    }
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)n, USART_TX_TIMEOUT_MS);
}

/* newlib 的 printf 最终调用 _write，重定向到 USART1 */
int _write(int file, char *ptr, int len)
{
    (void)file;
    if ((ptr != 0) && (len > 0)) {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)ptr, (uint16_t)len, USART_TX_TIMEOUT_MS);
    }
    return len;
}
