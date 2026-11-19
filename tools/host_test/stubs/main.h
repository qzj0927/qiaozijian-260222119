/**
 * @file    main.h  (PC 端编译检查用的 HAL 桩头文件)
 * @brief   只用于在 PC 上跑 `gcc -fsyntax-only`，验证我们自己的 .c 能编译过。
 *
 * 它不参与固件构建，也永远不会被 CubeMX 覆盖 —— 只放在 tools/host_test/stubs 下。
 * 函数签名按 STM32F4 HAL 的真实定义写，所以这里能过，基本就能在 Keil/CMake 里过。
 */
#ifndef HOST_STUB_MAIN_H
#define HOST_STUB_MAIN_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

/* ---------------- 基础类型 ---------------- */
typedef enum {
    HAL_OK = 0x00,
    HAL_ERROR = 0x01,
    HAL_BUSY = 0x02,
    HAL_TIMEOUT = 0x03
} HAL_StatusTypeDef;

typedef enum {
    GPIO_PIN_RESET = 0,
    GPIO_PIN_SET
} GPIO_PinState;

typedef struct {
    void *Instance;
} TIM_HandleTypeDef;

typedef struct {
    void *Instance;
} I2C_HandleTypeDef;

typedef struct {
    void *Instance;
} UART_HandleTypeDef;

typedef struct {
    uint32_t dummy;
} USBD_HandleTypeDef;

#define TIM_CHANNEL_1 0x00000000u
#define TIM_CHANNEL_2 0x00000004u
#define TIM_CHANNEL_3 0x00000008u
#define TIM_CHANNEL_4 0x0000000Cu

#define LED_Pin          (1u << 13)
#define LED_GPIO_Port    ((void *)0x40020800)
#define KEY_Pin          (1u << 12)
#define KEY_GPIO_Port    ((void *)0x40020400)

#define I2C_MEMADD_SIZE_8BIT 0x00000001u

#define RCC_FLAG_PORRST   0
#define RCC_FLAG_PINRST   1
#define RCC_FLAG_SFTRST   2
#define RCC_FLAG_IWDGRST  3
#define RCC_FLAG_WWDGRST  4
#define RCC_FLAG_LPWRRST  5
#define RCC_FLAG_BORRST   6

/* ---------------- 中断屏蔽 ---------------- */
static inline uint32_t __get_PRIMASK(void)
{
    return 0u;
}

static inline void __set_PRIMASK(uint32_t p)
{
    (void)p;
}

static inline void __disable_irq(void) { }
static inline void __enable_irq(void) { }

/* ---------------- 外设句柄（真实工程里由 CubeMX 定义） ---------------- */
extern TIM_HandleTypeDef  htim3;
extern I2C_HandleTypeDef  hi2c1;
extern UART_HandleTypeDef huart1;

/* ---------------- 函数桩 ---------------- */
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *htim, uint32_t channel);
HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *htim, uint32_t channel);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c, uint16_t dev, uint16_t reg,
                                   uint16_t mem_add_size, uint8_t *buf, uint16_t size,
                                   uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t dev, uint16_t reg,
                                    uint16_t mem_add_size, uint8_t *buf, uint16_t size,
                                    uint32_t timeout);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, uint8_t *data, uint16_t size,
                                    uint32_t timeout);
uint32_t HAL_GetTick(void);
void     HAL_Delay(uint32_t ms);
void     HAL_GPIO_WritePin(void *port, uint16_t pin, GPIO_PinState state);
void     HAL_GPIO_TogglePin(void *port, uint16_t pin);
GPIO_PinState HAL_GPIO_ReadPin(void *port, uint16_t pin);
uint32_t HAL_RCC_GetHCLKFreq(void);
uint32_t HAL_RCC_GetPCLK1Freq(void);

uint32_t __HAL_RCC_GET_FLAG(uint32_t flag);
void     __HAL_RCC_CLEAR_RESET_FLAGS(void);

#define __HAL_TIM_SET_COMPARE(htim, ch, value) ((void)(htim), (void)(ch), (void)(value))

#endif /* HOST_STUB_MAIN_H */
