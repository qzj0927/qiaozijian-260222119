/**
 * @file    main.h （PC 端语法检查用的 HAL 桩）
 * @brief   只用于 gcc -fsyntax-only 验证应用层代码能编译，
 *          不参与固件构建，不会被 CubeMX 覆盖。
 */
#ifndef HOST_STUB_MAIN_H
#define HOST_STUB_MAIN_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef enum { HAL_OK = 0, HAL_ERROR = 1, HAL_BUSY = 2, HAL_TIMEOUT = 3 } HAL_StatusTypeDef;

typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET } GPIO_PinState;

typedef struct { void *Instance; } TIM_HandleTypeDef;
typedef struct { void *Instance; } UART_HandleTypeDef;

#define TIM_CHANNEL_1 0x00u
#define TIM_CHANNEL_2 0x04u
#define TIM_CHANNEL_3 0x08u
#define TIM_CHANNEL_4 0x0Cu

#define TIM2           ((void *)0x40000000u)

#define LED_Pin        (1u << 13)
#define LED_GPIO_Port  ((void *)0x40020800)
#define KEY_Pin        (1u << 0)
#define KEY_GPIO_Port  ((void *)0x40020000)

/* CubeMX User Label 生成的小写宏（工程里实际是 led_Pin / key_Pin） */
#define led_Pin        LED_Pin
#define led_GPIO_Port  LED_GPIO_Port
#define key_Pin        KEY_Pin
#define key_GPIO_Port  KEY_GPIO_Port

HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *htim, uint32_t channel);
HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *htim, uint32_t channel);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, uint8_t *data, uint16_t size,
                                    uint32_t timeout);
uint32_t HAL_GetTick(void);
void HAL_GPIO_WritePin(void *port, uint16_t pin, GPIO_PinState state);
void HAL_GPIO_TogglePin(void *port, uint16_t pin);
GPIO_PinState HAL_GPIO_ReadPin(void *port, uint16_t pin);
uint32_t HAL_RCC_GetHCLKFreq(void);

#define __HAL_TIM_SET_COMPARE(htim, ch, value) ((void)(htim), (void)(ch), (void)(value))

#endif /* HOST_STUB_MAIN_H */
