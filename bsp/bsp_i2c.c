/**
 * @file    bsp_i2c.c
 * @brief   硬件 I2C 封装
 *
 * CubeMX 中 I2C1 配置：400kHz（Fast Mode），PB8=SCL/PB9=SDA，开内部上拉；
 * MPU6050 模块自带上拉时以模块为准（并联后等效上拉约 2.2k，仍可接受）。
 */
#include "bsp_i2c.h"

#include "main.h"

extern I2C_HandleTypeDef hi2c1;

static uint32_t s_error_count;

static bsp_i2c_status_t map_status(HAL_StatusTypeDef st)
{
    switch (st) {
    case HAL_OK:
        return BSP_I2C_OK;
    case HAL_TIMEOUT:
        return BSP_I2C_ERR_TIMEOUT;
    case HAL_ERROR:
        return BSP_I2C_ERR_NAK;
    default:
        return BSP_I2C_ERR_OTHER;
    }
}

void bsp_i2c_init(void)
{
    s_error_count = 0u;
}

bsp_i2c_status_t bsp_i2c_read(uint8_t dev_addr,
                              uint8_t reg,
                              uint8_t *buf,
                              uint16_t len,
                              uint32_t timeout_ms)
{
    HAL_StatusTypeDef st;

    if ((buf == 0) || (len == 0u)) {
        return BSP_I2C_ERR_OTHER;
    }

    st = HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(dev_addr << 1), reg,
                          I2C_MEMADD_SIZE_8BIT, buf, len, timeout_ms);
    if (st != HAL_OK) {
        ++s_error_count;
    }
    return map_status(st);
}

bsp_i2c_status_t bsp_i2c_write(uint8_t dev_addr,
                               uint8_t reg,
                               const uint8_t *buf,
                               uint16_t len,
                               uint32_t timeout_ms)
{
    HAL_StatusTypeDef st;

    if ((buf == 0) || (len == 0u)) {
        return BSP_I2C_ERR_OTHER;
    }

    st = HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(dev_addr << 1), reg,
                           I2C_MEMADD_SIZE_8BIT, (uint8_t *)buf, len, timeout_ms);
    if (st != HAL_OK) {
        ++s_error_count;
    }
    return map_status(st);
}

uint32_t bsp_i2c_error_count(void)
{
    return s_error_count;
}
