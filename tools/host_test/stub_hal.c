/**
 * @file    stub_hal.c
 * @brief   PC 端联调测试用的 HAL 桩实现
 *
 * 提供最小可用的 HAL 行为：可控毫秒时基、CDC 发送捕获、PWM 比较值记录。
 * 只在主机测试里链接，不参与固件构建。
 */
#include "main.h"
#include "bsp_usb_cdc.h"

#include <string.h>

#define USBD_OK 0u

TIM_HandleTypeDef  htim3;
I2C_HandleTypeDef  hi2c1;
UART_HandleTypeDef huart1;
USBD_HandleTypeDef hUsbDeviceFS;

static uint32_t s_ms;

void stub_set_ms(uint32_t ms)
{
    s_ms = ms;
}

void stub_advance_ms(uint32_t delta)
{
    s_ms += delta;
}

uint32_t HAL_GetTick(void)
{
    return s_ms;
}

void HAL_Delay(uint32_t ms)
{
    s_ms += ms;
}

/* ---- CDC 发送捕获 ---- */
#define STUB_TX_CAP 65536u
static uint8_t  s_tx[STUB_TX_CAP];
static uint32_t s_tx_len;

uint8_t CDC_Transmit_FS(uint8_t *buf, uint16_t len)
{
    if ((buf != 0) && ((s_tx_len + len) <= STUB_TX_CAP)) {
        (void)memcpy(&s_tx[s_tx_len], buf, len);
        s_tx_len += len;
    }
    bsp_usb_cdc_on_tx_complete();   /* 立即"发完"，让 pump 能继续推 */
    return USBD_OK;
}

const uint8_t *stub_tx_data(void)
{
    return s_tx;
}

uint32_t stub_tx_len(void)
{
    return s_tx_len;
}

void stub_tx_reset(void)
{
    s_tx_len = 0u;
}

/* ---- 其余桩 ---- */

HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *htim, uint32_t channel)
{
    (void)htim;
    (void)channel;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *htim, uint32_t channel)
{
    (void)htim;
    (void)channel;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c, uint16_t dev, uint16_t reg,
                                   uint16_t mem_add_size, uint8_t *buf, uint16_t size,
                                   uint32_t timeout)
{
    (void)hi2c;
    (void)dev;
    (void)reg;
    (void)mem_add_size;
    (void)buf;
    (void)size;
    (void)timeout;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t dev, uint16_t reg,
                                    uint16_t mem_add_size, uint8_t *buf, uint16_t size,
                                    uint32_t timeout)
{
    (void)hi2c;
    (void)dev;
    (void)reg;
    (void)mem_add_size;
    (void)buf;
    (void)size;
    (void)timeout;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, uint8_t *data, uint16_t size,
                                    uint32_t timeout)
{
    (void)huart;
    (void)data;
    (void)size;
    (void)timeout;
    return HAL_OK;
}

void HAL_GPIO_WritePin(void *port, uint16_t pin, GPIO_PinState state)
{
    (void)port;
    (void)pin;
    (void)state;
}

void HAL_GPIO_TogglePin(void *port, uint16_t pin)
{
    (void)port;
    (void)pin;
}

GPIO_PinState HAL_GPIO_ReadPin(void *port, uint16_t pin)
{
    (void)port;
    (void)pin;
    return GPIO_PIN_SET;
}

uint32_t HAL_RCC_GetHCLKFreq(void)
{
    return 84000000u;
}

uint32_t HAL_RCC_GetPCLK1Freq(void)
{
    return 42000000u;
}

uint32_t __HAL_RCC_GET_FLAG(uint32_t flag)
{
    (void)flag;
    return 0u;
}

void __HAL_RCC_CLEAR_RESET_FLAGS(void)
{
}
