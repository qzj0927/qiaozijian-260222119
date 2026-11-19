/**
 * @file    usbd_hid.h  (PC 端编译检查用桩文件)
 */
#ifndef HOST_STUB_USBD_HID_H
#define HOST_STUB_USBD_HID_H

#include <stdint.h>

#include "main.h"

#ifndef USBD_OK
#define USBD_OK   0u
#define USBD_BUSY 1u
#define USBD_FAIL 2u
#endif

uint8_t USBD_HID_SendReport(USBD_HandleTypeDef *pdev, uint8_t *report, uint16_t len);

#endif /* HOST_STUB_USBD_HID_H */
