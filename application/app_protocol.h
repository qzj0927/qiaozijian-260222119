/**
 * @file    app_protocol.h
 * @brief   协议应用层：收帧分发、状态上报、超时与故障判定
 *
 * 这一层是"协议"和"业务"的接缝：
 *   - 向下只依赖 usb_link（传输无关）与 protocol/arm_protocol（编解码）；
 *   - 向上调用 app_motion / app_servo / app_state；
 *   - 因此从 CDC 换到 HID 时，本文件一行都不用改。
 */
#ifndef APPLICATION_APP_PROTOCOL_H
#define APPLICATION_APP_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 初始化解析器与计时器 */
void app_protocol_init(void);

/** @brief 链路重连/复位后调用：清解析器、清超时，不改机械臂姿态 */
void app_protocol_reset_link(void);

/** @brief 喂入收到的字节流（CDC 分包、HID 整包都走这里） */
void app_protocol_feed(const uint8_t *data, uint32_t len);

/**
 * @brief 周期性调用（主循环每次循环调一次即可）
 * @param now_ms 当前毫秒
 * @note  负责：拉取 USB 数据、解析、超时判定、发送状态帧的节流判断
 */
void app_protocol_tick(uint32_t now_ms);

/**
 * @brief 上报 I2C 传感器状态（每 20ms 调一次）
 * @param ok     本次读是否成功
 * @param now_ms 当前毫秒
 */
void app_protocol_report_sensor(bool ok, uint32_t now_ms);

/** @brief 指令超时（bit5） */
bool app_protocol_cmd_timeout(void);

/** @brief 传感器故障（bit4） */
bool app_protocol_sensor_failed(void);

/** @brief 由传感器层写入最近一次俯仰角（0.1°），无传感器时填 0 */
void app_protocol_set_pitch(int16_t pitch_d10);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_APP_PROTOCOL_H */
