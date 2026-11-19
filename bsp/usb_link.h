/**
 * @file    usb_link.h
 * @brief   USB 传输层抽象（CDC / Custom HID 二选一）
 *
 * 任务书 4.2 要求"同一条协议帧在 CDC 上是变长字节流、在 HID 上是 64 字节报告，
 * 解析代码只能写一份"。做法就是这层接口：
 *
 *   应用层只调 usb_link_get()->write(...) / read(...)，
 *   具体是 CDC 还是 HID 由编译宏 USB_LINK_HID 在链接期决定。
 *
 * 这样评审把传输层从 CDC 换成 HID 时，protocol/ 与 application/ 里的代码
 * 一行都不用改，正好是他们要检查的点。
 */
#ifndef BSP_USB_LINK_H
#define BSP_USB_LINK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;                                    /**< "CDC" / "HID" */
    void     (*init)(void);                              /**< 初始化 */
    bool     (*ready)(void);                             /**< 主机是否已枚举并就绪 */
    uint32_t (*write)(const uint8_t *data, uint32_t len);/**< 发送一帧，返回入队字节数 */
    uint32_t (*read)(uint8_t *dst, uint32_t max_len);    /**< 取出收到的字节，返回实际字节数 */
    void     (*pump)(void);                              /**< 主循环里推动发送，非阻塞 */
} usb_link_t;

/** @brief 取当前编译配置对应的链路实现 */
const usb_link_t *usb_link_get(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_USB_LINK_H */
