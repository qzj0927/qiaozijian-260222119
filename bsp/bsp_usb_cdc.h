/**
 * @file    bsp_usb_cdc.h
 * @brief   USB CDC 虚拟串口传输层（T5a）
 *
 * 关键约束（全部来自任务书）：
 *  1. 状态帧 20~50Hz 连续发送，10 分钟不丢帧、不乱码、不断开；
 *  2. 发送缓冲满时丢弃最旧数据，绝不能阻塞控制节拍；
 *  3. 拔插 USB 后 1 秒内自动重连（枚举由 USB 库负责，这里负责清队列、重置解析器）。
 *
 * 实现：单生产者（主循环）单消费者（发送完成中断）的环形缓冲 + 64 字节分片。
 */
#ifndef BSP_BSP_USB_CDC_H
#define BSP_BSP_USB_CDC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 初始化，必须在 MX_USB_DEVICE_Init 之后调用 */
void bsp_usb_cdc_init(void);

/** @brief 主机是否已配置（枚举完成） */
bool bsp_usb_cdc_ready(void);

/** @brief 入队一帧，返回入队字节数；缓冲满时为 0 且写指针不动 */
uint32_t bsp_usb_cdc_write(const uint8_t *data, uint32_t len);

/** @brief 取出收到的字节，返回实际字节数 */
uint32_t bsp_usb_cdc_read(uint8_t *dst, uint32_t max_len);

/** @brief 主循环调用：把队列里的数据推给 USB，非阻塞 */
void bsp_usb_cdc_pump(void);

/* ---------- 以下由 USB 中断回调调用（改 usbd_cdc_if.c 时接上） ---------- */

/** @brief CDC_Receive_FS 收到数据 */
void bsp_usb_cdc_on_rx(const uint8_t *data, uint32_t len);

/** @brief CDC_TransmitCplt_FS 发送完成 */
void bsp_usb_cdc_on_tx_complete(void);

/** @brief CDC_Init_FS / 重新枚举完成 */
void bsp_usb_cdc_on_configured(void);

/** @brief 拔线/复位 */
void bsp_usb_cdc_on_reset(void);

/** @brief 因缓冲满丢弃的字节数（自检打印用） */
uint32_t bsp_usb_cdc_dropped_bytes(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BSP_USB_CDC_H */
