/**
 * @file    bsp_usb_cdc.c
 * @brief   CDC 虚拟串口传输层实现
 *
 * 为什么要自己再做一层环形缓冲？
 *   CDC_Transmit_FS 在上一包没发完时返回 USBD_BUSY，而上位机 20~50Hz 的状态帧
 *   必须"绝不阻塞控制节拍"。所以这里用"主循环入队 + 中断置完成标志 + 主循环分片发送"，
 *   入队永远是内存拷贝（几十字节），最坏情况是丢一帧，绝不会卡住 200Hz 的插补。
 */
#include "bsp_usb_cdc.h"

#include "usb_link.h"
#include "usbd_cdc_if.h"

#include "main.h"

#define CDC_TX_CAP   2048u   /* 必须为 2 的幂 */
#define CDC_RX_CAP    512u   /* 必须为 2 的幂 */
#define CDC_EP_MAX     64u   /* 全速 Bulk 端点最大包长 */

static uint8_t s_tx[CDC_TX_CAP];
static uint8_t s_rx[CDC_RX_CAP];

static volatile uint16_t s_tx_head;
static volatile uint16_t s_tx_tail;
static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;
static volatile bool     s_ready;
static volatile bool     s_tx_busy;
static volatile uint32_t s_dropped;

static uint32_t enter_critical(void)
{
    const uint32_t primask = __get_PRIMASK();

    __disable_irq();
    return primask;
}

static void exit_critical(uint32_t primask)
{
    __set_PRIMASK(primask);
}

static uint16_t ring_count(volatile uint16_t *head, volatile uint16_t *tail)
{
    return (uint16_t)(*head - *tail);
}

void bsp_usb_cdc_init(void)
{
    s_tx_head  = 0u;
    s_tx_tail  = 0u;
    s_rx_head  = 0u;
    s_rx_tail  = 0u;
    s_tx_busy  = false;
    s_dropped  = 0u;
    s_ready    = false;   /* 等主机枚举完成后由 bsp_usb_cdc_on_configured 置位 */
}

bool bsp_usb_cdc_ready(void)
{
    return s_ready;
}

uint32_t bsp_usb_cdc_write(const uint8_t *data, uint32_t len)
{
    uint32_t primask;

    if ((!s_ready) || (data == 0) || (len == 0u)) {
        return 0u;
    }

    primask = enter_critical();
    for (uint32_t i = 0u; i < len; ++i) {
        /* 缓冲满：丢最旧的一字节（任务书 T5a 的明确要求），不等待、不阻塞 */
        if (ring_count(&s_tx_head, &s_tx_tail) >= (CDC_TX_CAP - 1u)) {
            s_tx_tail = (uint16_t)(s_tx_tail + 1u);
            ++s_dropped;
        }
        s_tx[s_tx_head & (CDC_TX_CAP - 1u)] = data[i];
        s_tx_head = (uint16_t)(s_tx_head + 1u);
    }
    exit_critical(primask);

    return len;
}

uint32_t bsp_usb_cdc_read(uint8_t *dst, uint32_t max_len)
{
    uint32_t n = 0u;
    uint32_t primask;

    if ((dst == 0) || (max_len == 0u) || (!s_ready)) {
        return 0u;
    }

    primask = enter_critical();
    while ((n < max_len) && (s_rx_head != s_rx_tail)) {
        dst[n] = s_rx[s_rx_tail & (CDC_RX_CAP - 1u)];
        s_rx_tail = (uint16_t)(s_rx_tail + 1u);
        ++n;
    }
    exit_critical(primask);

    return n;
}

void bsp_usb_cdc_pump(void)
{
    uint8_t  chunk[CDC_EP_MAX];
    uint32_t n;
    uint16_t idx;
    uint16_t count;
    uint32_t primask;

    if ((!s_ready) || s_tx_busy) {
        return;
    }

    primask = enter_critical();
    count = ring_count(&s_tx_head, &s_tx_tail);
    idx   = (uint16_t)(s_tx_tail & (CDC_TX_CAP - 1u));
    n     = (count > CDC_EP_MAX) ? CDC_EP_MAX : count;
    for (uint32_t i = 0u; i < n; ++i) {
        chunk[i] = s_tx[(idx + i) & (CDC_TX_CAP - 1u)];
    }
    exit_critical(primask);

    if (n == 0u) {
        return;
    }

    /* 注意：CDC_Transmit_FS 内部会保存缓冲区指针，因此必须先把数据拷到栈上，
     * 不能把环形缓冲区的地址直接传给它。
     *
     * 先置 busy 再调用：发送完成回调有可能在 CDC_Transmit_FS 内部同步触发，
     * 如果先调用后置位，回调清掉的标志会被我们又置回去，从此再也发不出数据。 */
    s_tx_busy = true;
    if (CDC_Transmit_FS(chunk, (uint16_t)n) != (uint8_t)USBD_OK) {
        s_tx_busy = false;
        return;
    }

    primask = enter_critical();
    s_tx_tail = (uint16_t)(s_tx_tail + (uint16_t)n);
    exit_critical(primask);
}

void bsp_usb_cdc_on_rx(const uint8_t *data, uint32_t len)
{
    uint32_t primask;

    if ((data == 0) || (len == 0u)) {
        return;
    }

    primask = enter_critical();
    for (uint32_t i = 0u; i < len; ++i) {
        if (ring_count(&s_rx_head, &s_rx_tail) >= (CDC_RX_CAP - 1u)) {
            s_rx_tail = (uint16_t)(s_rx_tail + 1u);   /* 接收也丢最旧，保证最新指令能进 */
            ++s_dropped;
        }
        s_rx[s_rx_head & (CDC_RX_CAP - 1u)] = data[i];
        s_rx_head = (uint16_t)(s_rx_head + 1u);
    }
    exit_critical(primask);
}

void bsp_usb_cdc_on_tx_complete(void)
{
    s_tx_busy = false;
}

void bsp_usb_cdc_on_configured(void)
{
    uint32_t primask = enter_critical();

    s_tx_head = 0u;
    s_tx_tail = 0u;
    s_rx_head = 0u;
    s_rx_tail = 0u;
    s_tx_busy = false;
    exit_critical(primask);

    s_ready = true;
}

void bsp_usb_cdc_on_reset(void)
{
    s_ready   = false;
    s_tx_busy = false;
}

uint32_t bsp_usb_cdc_dropped_bytes(void)
{
    return s_dropped;
}

/* ---------------- 链路实现表 ---------------- */

const usb_link_t g_usb_link_cdc = {
    .name  = "CDC",
    .init  = bsp_usb_cdc_init,
    .ready = bsp_usb_cdc_ready,
    .write = bsp_usb_cdc_write,
    .read  = bsp_usb_cdc_read,
    .pump  = bsp_usb_cdc_pump
};
