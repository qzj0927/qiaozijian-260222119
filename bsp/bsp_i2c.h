/**
 * @file    bsp_i2c.h
 * @brief   硬件 I2C 收发（400kHz，带超时与错误分类）
 *
 * 任务书 T4 要求 I2C 出现 NAK/超时/断线时 1 秒内进入安全态，
 * 所以这里不返回 HAL 的状态码，而是把它翻译成"可判断业务含义"的三态。
 */
#ifndef BSP_BSP_I2C_H
#define BSP_BSP_I2C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BSP_I2C_OK = 0,        /**< 成功 */
    BSP_I2C_ERR_NAK,       /**< 从机不应答（掉线、地址错、SDA 被拉低） */
    BSP_I2C_ERR_TIMEOUT,   /**< 总线超时（SCL 被拉低、上拉不足） */
    BSP_I2C_ERR_OTHER      /**< 其他错误（总线忙、仲裁丢失） */
} bsp_i2c_status_t;

/** @brief 初始化（HAL 已由 CubeMX 完成，这里只清计数器） */
void bsp_i2c_init(void);

/** @brief 读寄存器 */
bsp_i2c_status_t bsp_i2c_read(uint8_t dev_addr,
                              uint8_t reg,
                              uint8_t *buf,
                              uint16_t len,
                              uint32_t timeout_ms);

/** @brief 写寄存器 */
bsp_i2c_status_t bsp_i2c_write(uint8_t dev_addr,
                               uint8_t reg,
                               const uint8_t *buf,
                               uint16_t len,
                               uint32_t timeout_ms);

/** @brief 累计错误次数，用于自检打印与故障统计 */
uint32_t bsp_i2c_error_count(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BSP_I2C_H */
