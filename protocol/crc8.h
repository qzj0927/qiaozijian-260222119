/**
 * @file    crc8.h
 * @brief   CRC8 校验（任务书附件一 第 7 节）
 *
 * 参数固定：多项式 0x07，初值 0x00，输入/输出均不反射，结果不异或。
 * 全工程只有这一份实现，禁止在别处再写一遍。
 */
#ifndef PROTOCOL_CRC8_H
#define PROTOCOL_CRC8_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  计算一段内存的 CRC8
 * @param  buf 数据首地址，允许为 NULL（此时 len 必须为 0）
 * @param  len 字节数
 * @return CRC8 结果
 */
uint8_t crc8_calc(const uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_CRC8_H */
