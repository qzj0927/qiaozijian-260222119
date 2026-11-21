/**
 * @file    bsp_usart.h
 * @brief   调试串口（USART1，115200 8N1）
 *
 * 只用于自检打印与调试，不要用它传上位机数据。
 */
#ifndef BSP_USART_H
#define BSP_USART_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 初始化（HAL 已由 MX_USART1_UART_Init 完成，这里只清状态） */
void bsp_usart_init(void);

/** @brief 阻塞发送字符串，最长等 50ms */
void bsp_usart_puts(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* BSP_USART_H */
