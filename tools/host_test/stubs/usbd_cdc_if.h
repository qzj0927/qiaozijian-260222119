/**
 * @file    usbd_cdc_if.h  (PC 端编译检查用桩文件)
 */
#ifndef HOST_STUB_USBD_CDC_IF_H
#define HOST_STUB_USBD_CDC_IF_H

#include <stdint.h>

#define USBD_OK     0u
#define USBD_BUSY   1u
#define USBD_FAIL   2u

uint8_t CDC_Transmit_FS(uint8_t *buf, uint16_t len);

#endif /* HOST_STUB_USBD_CDC_IF_H */
