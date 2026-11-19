/**
 * @file    bsp_usb_hid.c
 * @brief   Custom HID 传输层实现
 *
 * 说明：HID 的 IN 报告只能由固件在主机轮询时送出，因此这里同样是
 * "主循环组帧 + 忙标志"，一次只挂一帧，发送完成后才允许下一帧。
 * 30Hz 的帧率远低于 1kHz 的轮询率，正常情况下不会丢帧。
 *
 * 收发各用一条独立队列：发送队列 2 帧（1 帧在飞 + 1 帧排队），
 * 接收队列 2 帧，避免"收到的帧被当成待发送帧回显出去"。
 */
#include "bsp_usb_hid.h"

#include "usb_link.h"
#include "usbd_hid.h"

#include "main.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

#define HID_QUEUE_LEN 2u

static uint8_t s_tx_q[HID_QUEUE_LEN][BSP_HID_REPORT_LEN];
static uint8_t s_rx_q[HID_QUEUE_LEN][BSP_HID_REPORT_LEN];
static uint8_t s_inflight[BSP_HID_REPORT_LEN];

static volatile uint8_t s_tx_head;
static volatile uint8_t s_tx_tail;
static volatile uint8_t s_rx_head;
static volatile uint8_t s_rx_tail;
static volatile bool    s_ready;
static volatile bool    s_tx_busy;
static volatile uint32_t s_dropped;

void bsp_usb_hid_init(void)
{
    s_tx_head = 0u;
    s_tx_tail = 0u;
    s_rx_head = 0u;
    s_rx_tail = 0u;
    s_tx_busy = false;
    s_dropped = 0u;
    s_ready   = false;
}

bool bsp_usb_hid_ready(void)
{
    return s_ready;
}

uint32_t bsp_usb_hid_write(const uint8_t *data, uint32_t len)
{
    uint8_t  slot;
    uint32_t primask;

    if ((!s_ready) || (data == 0) || (len == 0u) || (len > BSP_HID_REPORT_LEN)) {
        return 0u;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    if ((uint8_t)(s_tx_head - s_tx_tail) >= HID_QUEUE_LEN) {
        s_tx_tail = (uint8_t)((s_tx_tail + 1u) % HID_QUEUE_LEN);   /* 丢最旧 */
        ++s_dropped;
    }

    slot = (uint8_t)(s_tx_head % HID_QUEUE_LEN);
    for (uint32_t i = 0u; i < BSP_HID_REPORT_LEN; ++i) {
        s_tx_q[slot][i] = (i < len) ? data[i] : 0u;   /* 余位补 0 */
    }
    s_tx_head = (uint8_t)((s_tx_head + 1u) % HID_QUEUE_LEN);

    __set_PRIMASK(primask);

    return len;
}

uint32_t bsp_usb_hid_read(uint8_t *dst, uint32_t max_len)
{
    uint32_t n;
    uint32_t primask;

    if ((dst == 0) || (max_len == 0u) || (!s_ready)) {
        return 0u;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    if (s_rx_head == s_rx_tail) {
        __set_PRIMASK(primask);
        return 0u;
    }

    n = (max_len < BSP_HID_REPORT_LEN) ? max_len : BSP_HID_REPORT_LEN;
    for (uint32_t i = 0u; i < n; ++i) {
        dst[i] = s_rx_q[s_rx_tail][i];
    }
    s_rx_tail = (uint8_t)((s_rx_tail + 1u) % HID_QUEUE_LEN);

    __set_PRIMASK(primask);

    return n;
}

void bsp_usb_hid_pump(void)
{
    uint8_t slot;

    if ((!s_ready) || s_tx_busy) {
        return;
    }
    if (s_tx_head == s_tx_tail) {
        return;
    }

    slot = (uint8_t)(s_tx_tail % HID_QUEUE_LEN);

    /* 先把待发帧拷到在飞缓冲区：USBD_HID_SendReport 是异步的，
     * 直接传队列地址会在下一帧入队时被改写 */
    for (uint32_t i = 0u; i < BSP_HID_REPORT_LEN; ++i) {
        s_inflight[i] = s_tx_q[slot][i];
    }

    /* 同样的顺序要求：先置 busy 再发送，避免完成回调被"盖掉" */
    s_tx_busy = true;
    if (USBD_HID_SendReport(&hUsbDeviceFS, s_inflight, BSP_HID_REPORT_LEN) != (uint8_t)USBD_OK) {
        s_tx_busy = false;
        return;
    }

    s_tx_tail = (uint8_t)((s_tx_tail + 1u) % HID_QUEUE_LEN);
}

void bsp_usb_hid_on_rx(const uint8_t *report, uint32_t len)
{
    uint8_t  slot;
    uint32_t primask;

    if ((report == 0) || (len < BSP_HID_REPORT_LEN)) {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    if ((uint8_t)(s_rx_head - s_rx_tail) >= HID_QUEUE_LEN) {
        s_rx_tail = (uint8_t)((s_rx_tail + 1u) % HID_QUEUE_LEN);
        ++s_dropped;
    }
    slot = (uint8_t)(s_rx_head % HID_QUEUE_LEN);
    for (uint32_t i = 0u; i < BSP_HID_REPORT_LEN; ++i) {
        s_rx_q[slot][i] = report[i];
    }
    s_rx_head = (uint8_t)((s_rx_head + 1u) % HID_QUEUE_LEN);

    __set_PRIMASK(primask);
}

void bsp_usb_hid_on_tx_complete(void)
{
    s_tx_busy = false;
}

void bsp_usb_hid_on_configured(void)
{
    s_tx_head = 0u;
    s_tx_tail = 0u;
    s_rx_head = 0u;
    s_rx_tail = 0u;
    s_tx_busy = false;
    s_ready   = true;
}

void bsp_usb_hid_on_reset(void)
{
    s_ready   = false;
    s_tx_busy = false;
}

uint32_t bsp_usb_hid_dropped_frames(void)
{
    return s_dropped;
}

const usb_link_t g_usb_link_hid = {
    .name  = "HID",
    .init  = bsp_usb_hid_init,
    .ready = bsp_usb_hid_ready,
    .write = bsp_usb_hid_write,
    .read  = bsp_usb_hid_read,
    .pump  = bsp_usb_hid_pump
};
