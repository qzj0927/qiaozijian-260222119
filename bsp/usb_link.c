/**
 * @file    usb_link.c
 * @brief   链路选择：编译期二选一
 *
 * 构建时用 -DUSB_LINK_HID=1 切到 Custom HID（T5b），默认 CDC（T5a 必做）。
 * 两个 .c 文件都由 cmake/app_sources.cmake 按同一宏挑选，不会同时链接。
 */
#include "usb_link.h"

#ifdef USB_LINK_HID
#define USB_LINK_IS_HID 1
#else
#define USB_LINK_IS_HID 0
#endif

#if (USB_LINK_IS_HID == 1)
extern const usb_link_t g_usb_link_hid;
#else
extern const usb_link_t g_usb_link_cdc;
#endif

const usb_link_t *usb_link_get(void)
{
#if (USB_LINK_IS_HID == 1)
    return &g_usb_link_hid;
#else
    return &g_usb_link_cdc;
#endif
}
