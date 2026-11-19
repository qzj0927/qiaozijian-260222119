/**
 * @file    bsp_usb_hid.h
 * @brief   Custom HID 双向传输层（T5b，选做）
 *
 * 报告描述符：厂商自定义页 0xFF00，报告 ID 0（即不带 ID 前缀），
 *   64 字节 Input + 64 字节 Output。
 *
 * 上位机侧看到的是"每次 64 字节"，而协议帧是变长的：
 *   - 发送：帧放在报告最前面，剩余补 0；
 *   - 接收：一次 OUT 报告只装一帧（任务书 5.3：一次传输只放一帧，不允许多帧拼接）。
 *
 * 与 CDC 共用 protocol/ 的解析代码，这正是"传输层与协议分离"的验证点。
 */
#ifndef BSP_BSP_USB_HID_H
#define BSP_BSP_USB_HID_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_HID_REPORT_LEN 64u

/** @brief 初始化，必须在 MX_USB_DEVICE_Init 之后调用 */
void bsp_usb_hid_init(void);

/** @brief 主机是否已配置 */
bool bsp_usb_hid_ready(void);

/** @brief 发一帧（不足 64 字节补 0），返回入队字节数 */
uint32_t bsp_usb_hid_write(const uint8_t *data, uint32_t len);

/** @brief 取出收到的报告内容（每次最多一个 64 字节报告） */
uint32_t bsp_usb_hid_read(uint8_t *dst, uint32_t max_len);

/** @brief 主循环推动发送 */
void bsp_usb_hid_pump(void);

/* ---------- 由 USB 回调调用 ---------- */

/** @brief 收到 OUT 报告（HID 类回调里把 64 字节转进来） */
void bsp_usb_hid_on_rx(const uint8_t *report, uint32_t len);

/** @brief 发送完成 */
void bsp_usb_hid_on_tx_complete(void);

/** @brief 枚举完成 */
void bsp_usb_hid_on_configured(void);

/** @brief 拔线/复位 */
void bsp_usb_hid_on_reset(void);

/** @brief 因队列满丢弃的帧数（自检打印用） */
uint32_t bsp_usb_hid_dropped_frames(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BSP_USB_HID_H */
