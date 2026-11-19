/**
 * @file    app_protocol.c
 * @brief   协议应用层实现
 *
 * 三条硬性时序规则（任务书第 6 节）都在 app_protocol_tick / 回调里落实：
 *   - 连续 3s 无 0x20：保持当前姿态（abort 后停在当前角）+ 置 bit5；
 *   - 连续 3s 无心跳：同上；
 *   - I2C 读失败累计 1s：停止运动 + 置 bit4/bit2（在 report_sensor 里做）。
 * CRC 错误的帧由解析器丢弃，不回复、不计入超时，所以这里看不到任何处理。
 */
#include "app_protocol.h"

#include "app_config.h"
#include "app_motion.h"
#include "app_servo.h"
#include "app_state.h"
#include "arm_protocol.h"
#include "bsp_time.h"
#include "usb_link.h"

#include <string.h>

static arm_parser_t s_parser;

static struct {
    uint32_t last_cmd_ms;
    uint32_t last_hb_ms;
    uint32_t last_status_ms;
    uint32_t fail_start_ms;
    uint32_t ok_start_ms;
    bool     link_active;
    bool     cmd_timeout;
    bool     hb_timeout;
    bool     sensor_fail;
    bool     fail_counting;
    bool     ok_counting;
    int16_t  pitch_d10;
} s_proto;

/* ---------------- 内部：发送 ---------------- */

static void send_frame(uint8_t type, const uint8_t *payload, uint8_t len)
{
    uint8_t  buf[3u + ARM_MAX_PAYLOAD + 1u];
    uint32_t n = arm_encode(type, payload, len, buf, sizeof(buf));

    if (n > 0u) {
        (void)usb_link_get()->write(buf, n);
    }
}

static void send_status(uint32_t now_ms)
{
    arm_status_t st;
    uint8_t      bits = 0u;
    uint8_t      buf[3u + ARM_LEN_STATUS + 1u];
    uint32_t     n;

    (void)now_ms;

    for (uint32_t i = 0; i < (uint32_t)ARM_AXIS_COUNT; ++i) {
        st.q_act[i] = app_servo_angle(i);
        st.q_tgt[i] = app_servo_target(i);
    }
    st.pitch = s_proto.pitch_d10;

    if (app_state_mode() == APP_MODE_AUTO) {
        bits |= ARM_ST_AUTO;
    }
    if (app_state_is_homing()) {
        bits |= ARM_ST_HOMING;
    }
    /* bit2 在本工程里表示"故障 / 受限"：急停、I2C 失效、以及目标被限位钳位。
     * 前两者需要显式复位才能清除，钳位则在收到下一个合法目标时自动清除。 */
    if (app_state_is_fault() || s_proto.sensor_fail || app_servo_any_limited()) {
        bits |= ARM_ST_FAULT;
    }
    if (app_motion_busy()) {
        bits |= ARM_ST_MOVING;
    }
    if (s_proto.sensor_fail) {
        bits |= ARM_ST_I2C_FAIL;
    }
    if (s_proto.cmd_timeout || s_proto.hb_timeout) {
        bits |= ARM_ST_CMD_TIMEOUT;
    }
    st.status = bits;

    n = arm_encode_status(&st, buf, sizeof(buf));
    if (n > 0u) {
        (void)usb_link_get()->write(buf, n);
    }
}

/* ---------------- 内部：收帧处理 ---------------- */

static void on_target(const arm_target_t *tgt, uint32_t now_ms)
{
    s_proto.last_cmd_ms  = now_ms;
    s_proto.cmd_timeout  = false;
    s_proto.link_active  = true;

    if (app_state_is_fault()) {
        return;   /* 故障态只接受 0x21=3 复位，其他指令一律不执行 */
    }
    if (!app_servo_ready()) {
        return;
    }

    (void)app_motion_start(tgt->q_tgt, tgt->duration_ms, tgt->profile);
}

static void on_mode(uint8_t mode, uint32_t now_ms)
{
    s_proto.link_active = true;
    s_proto.last_cmd_ms = now_ms;
    s_proto.cmd_timeout = false;

    switch (mode) {
    case ARM_MODE_MANUAL:
        (void)app_state_request(APP_MODE_MANUAL);
        break;
    case ARM_MODE_AUTO:
        (void)app_state_request(APP_MODE_AUTO);
        break;
    case ARM_MODE_HOMING:
        (void)app_state_request(APP_MODE_HOMING);
        break;
    case ARM_MODE_FAULT_RESET:
        if (app_state_is_fault()) {
            (void)app_state_clear_fault();
            app_servo_clear_limits();
        }
        break;
    default:
        break;
    }
}

static void on_estop(uint8_t flag)
{
    if (flag == 0u) {
        return;   /* 0 为保留值，不做任何事 */
    }
    app_state_enter_fault();
}

static void dispatch(const arm_frame_t *frame, uint32_t now_ms)
{
    switch (frame->type) {
    case ARM_MSG_TARGET: {
        arm_target_t tgt;

        if (arm_decode_target(frame, &tgt)) {
            on_target(&tgt, now_ms);
        }
        break;
    }
    case ARM_MSG_MODE:
        if (frame->len == ARM_LEN_MODE) {
            on_mode(frame->payload[0], now_ms);
        }
        break;
    case ARM_MSG_ESTOP:
        if (frame->len == ARM_LEN_ESTOP) {
            on_estop(frame->payload[0]);
        }
        break;
    case ARM_MSG_HEARTBEAT:
        /* 原样回发：上位机用它算往返时延与丢包 */
        if (frame->len == ARM_LEN_HEARTBEAT) {
            s_proto.last_hb_ms = now_ms;
            s_proto.hb_timeout = false;
            s_proto.link_active = true;
            send_frame(ARM_MSG_HEARTBEAT, frame->payload, 1u);
        }
        break;
    case ARM_MSG_VERSION: {
        uint8_t ver[2] = {ARM_VERSION_MAJOR, ARM_VERSION_MINOR};

        s_proto.link_active = true;
        send_frame(ARM_MSG_VERSION, ver, 2u);
        break;
    }
    default:
        /* 未知类型忽略，不回复：避免和未来扩展打架 */
        break;
    }
}

/* ---------------- 对外接口 ---------------- */

void app_protocol_init(void)
{
    const uint32_t now = bsp_time_ms();

    (void)memset(&s_proto, 0, sizeof(s_proto));
    arm_parser_init(&s_parser);

    s_proto.last_cmd_ms    = now;
    s_proto.last_hb_ms     = now;
    s_proto.last_status_ms = now - APP_STATUS_PERIOD_MS;   /* 上电立刻发一帧 */
    s_proto.pitch_d10      = 0;
}

void app_protocol_reset_link(void)
{
    const uint32_t now = bsp_time_ms();

    arm_parser_init(&s_parser);
    s_proto.last_cmd_ms = now;
    s_proto.last_hb_ms  = now;
    s_proto.cmd_timeout = false;
    s_proto.hb_timeout  = false;
    s_proto.link_active = false;
}

void app_protocol_feed(const uint8_t *data, uint32_t len)
{
    arm_frame_t frame;
    const uint32_t now = bsp_time_ms();

    if ((data == 0) || (len == 0u)) {
        return;
    }

    for (uint32_t i = 0; i < len; ++i) {
        const int r = arm_parser_push(&s_parser, data[i], &frame);

        if (r == 1) {
            dispatch(&frame, now);
        }
        /* r == -1：CRC 错或长度非法，已在解析器里复位，这里什么都不做 */
    }
}

void app_protocol_tick(uint32_t now_ms)
{
    const usb_link_t *link = usb_link_get();
    uint8_t           buf[64];
    uint32_t          n;

    /* 1. 把 USB 收到的字节全部喂给解析器（一帧可能跨多次 read） */
    while ((n = link->read(buf, sizeof(buf))) > 0u) {
        app_protocol_feed(buf, n);
    }

    /* 2. 超时判定：只在链路曾经活跃过之后才计时，避免上电就报 bit5 */
    if (s_proto.link_active) {
        if (bsp_time_passed(s_proto.last_cmd_ms, now_ms, APP_CMD_TIMEOUT_MS)) {
            if (!s_proto.cmd_timeout) {
                s_proto.cmd_timeout = true;
                app_motion_abort();   /* "保持当前姿态"：停在当前角，不继续走 */
            }
        }
        if (bsp_time_passed(s_proto.last_hb_ms, now_ms, APP_HB_TIMEOUT_MS)) {
            s_proto.hb_timeout = true;
        }
    }

    /* 3. 状态帧节流发送 */
    if (bsp_time_passed(s_proto.last_status_ms, now_ms, APP_STATUS_PERIOD_MS)) {
        s_proto.last_status_ms = now_ms;
        send_status(now_ms);
    }

    /* 4. 推动发送队列（CDC 分片 / HID 报告） */
    link->pump();
}

void app_protocol_report_sensor(bool ok, uint32_t now_ms)
{
    if (ok) {
        s_proto.fail_counting = false;
        if (s_proto.sensor_fail) {
            if (!s_proto.ok_counting) {
                s_proto.ok_counting  = true;
                s_proto.ok_start_ms  = now_ms;
            } else if (bsp_time_passed(s_proto.ok_start_ms, now_ms, APP_I2C_RECOVER_MS)) {
                s_proto.sensor_fail = false;   /* 重新接入即可恢复，不需要复位 */
                s_proto.ok_counting = false;
            }
        }
        return;
    }

    s_proto.ok_counting = false;
    if (s_proto.sensor_fail) {
        return;
    }
    if (!s_proto.fail_counting) {
        s_proto.fail_counting  = true;
        s_proto.fail_start_ms  = now_ms;
    } else if (bsp_time_passed(s_proto.fail_start_ms, now_ms, APP_I2C_FAULT_MS)) {
        s_proto.sensor_fail = true;   /* 1s 到点：停止运动 + bit4/bit2 */
        app_motion_abort();
    }
}

bool app_protocol_cmd_timeout(void)
{
    return s_proto.cmd_timeout || s_proto.hb_timeout;
}

bool app_protocol_sensor_failed(void)
{
    return s_proto.sensor_fail;
}

void app_protocol_set_pitch(int16_t pitch_d10)
{
    s_proto.pitch_d10 = pitch_d10;
}
