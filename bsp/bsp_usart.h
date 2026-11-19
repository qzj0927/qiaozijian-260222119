/**
 * @file    bsp_usart.h
 * @brief   调试串口（USART1，115200 8N1）
 *
 * 用途：T0 自检打印、标定过程打印、故障码打印。
 * 注意：USB CDC 才是给上位机的数据通道，调试信息绝不能走 USB（任务书 T5a 明确禁止
 *       "用 printf 走 USB"），两者分开正是为了这一点。
 */
#ifndef BSP_BSP_USART_H
#define BSP_BSP_USART_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 初始化（HAL 已由 CubeMX 完成，这里清状态） */
void bsp_usart_init(void);

/** @brief 阻塞发送字符串，最长等待 50ms，超时即丢（绝不无限等） */
void bsp_usart_puts(const char *text);

/** @brief 格式化打印，等价于 printf，但只在调试口输出 */
void bsp_usart_printf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BSP_USART_H */
