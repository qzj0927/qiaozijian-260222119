/**
 * @file    crc8.c
 * @brief   CRC8 逐位实现
 *
 * 采用逐位算法而不是查表：50Hz 下每帧 20 字节左右，逐位开销可忽略，
 * 换来的是不需要 256 字节常量表、代码一眼可核对。若以后帧率上去了，
 * 再换半字节查表法（16 项表），接口不变。
 */
#include "crc8.h"

uint8_t crc8_calc(const uint8_t *buf, uint32_t len)
{
    uint8_t crc = 0x00u;

    if (buf == 0) {
        return 0x00u;
    }

    while (len-- > 0u) {
        crc ^= *buf++;
        for (uint8_t i = 0u; i < 8u; ++i) {
            if ((crc & 0x80u) != 0u) {
                crc = (uint8_t)((uint8_t)(crc << 1) ^ 0x07u);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}
