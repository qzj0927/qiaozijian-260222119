/**
 * @file    arm_protocol.c
 * @brief   协议编解码与字节流解析
 *
 * 设计要点：
 *  1. 所有多字节字段按小端逐字节拼装（不用结构体指针强转），
 *     这样与编译器对齐方式、CPU 字节序彻底无关，上位机换成任何语言都能对上。
 *  2. 解码时严格校验载荷长度：长度不符直接丢帧，避免"半帧"被当成合法指令执行。
 *  3. 解析器按长度字段切包，因此载荷中出现 0xA5 也不会误同步。
 */
#include "arm_protocol.h"
#include "crc8.h"

#include <stddef.h>
#include <string.h>

#define ARM_RAW_HEAD_LEN 3u  /* 帧头 + 类型 + 长度 */

/* ---------------- 小端读写 ---------------- */

static uint16_t read_u16_le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}

static void write_u16_le(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value & 0xFFu);
    p[1] = (uint8_t)(value >> 8);
}

/* ---------------- 编码 ---------------- */

uint32_t arm_encode(uint8_t type,
                    const uint8_t *payload,
                    uint8_t len,
                    uint8_t *out,
                    uint32_t out_size)
{
    uint32_t total;

    if (out == NULL) {
        return 0u;
    }
    if (len > ARM_MAX_PAYLOAD) {
        return 0u;
    }
    if ((len > 0u) && (payload == NULL)) {
        return 0u;
    }

    total = (uint32_t)len + ARM_FRAME_OVERHEAD;
    if (out_size < total) {
        return 0u;
    }

    out[0] = ARM_FRAME_HEAD;
    out[1] = type;
    out[2] = len;
    if (len > 0u) {
        (void)memcpy(&out[3], payload, (size_t)len);
    }
    out[ARM_RAW_HEAD_LEN + len] = crc8_calc(out, ARM_RAW_HEAD_LEN + (uint32_t)len);

    return total;
}

uint32_t arm_encode_status(const arm_status_t *st, uint8_t *out, uint32_t out_size)
{
    uint8_t payload[ARM_LEN_STATUS];

    if (st == NULL) {
        return 0u;
    }

    for (uint32_t i = 0u; i < (uint32_t)ARM_AXIS_COUNT; ++i) {
        write_u16_le(&payload[i * 2u], (uint16_t)st->q_act[i]);
    }
    for (uint32_t i = 0u; i < (uint32_t)ARM_AXIS_COUNT; ++i) {
        write_u16_le(&payload[8u + i * 2u], (uint16_t)st->q_tgt[i]);
    }
    write_u16_le(&payload[16], (uint16_t)st->pitch);
    payload[18] = st->status;

    return arm_encode(ARM_MSG_STATUS, payload, (uint8_t)sizeof(payload), out, out_size);
}

uint32_t arm_encode_version(uint8_t *out, uint32_t out_size)
{
    uint8_t payload[2];

    payload[0] = ARM_VERSION_MAJOR;
    payload[1] = ARM_VERSION_MINOR;

    return arm_encode(ARM_MSG_VERSION, payload, (uint8_t)sizeof(payload), out, out_size);
}

/* ---------------- 解码 ---------------- */

bool arm_decode_status(const arm_frame_t *frame, arm_status_t *st)
{
    if ((frame == NULL) || (st == NULL)) {
        return false;
    }
    if ((frame->type != ARM_MSG_STATUS) || (frame->len != ARM_LEN_STATUS)) {
        return false;
    }

    for (uint32_t i = 0u; i < (uint32_t)ARM_AXIS_COUNT; ++i) {
        st->q_act[i] = (int16_t)read_u16_le(&frame->payload[i * 2u]);
    }
    for (uint32_t i = 0u; i < (uint32_t)ARM_AXIS_COUNT; ++i) {
        st->q_tgt[i] = (int16_t)read_u16_le(&frame->payload[8u + i * 2u]);
    }
    st->pitch  = (int16_t)read_u16_le(&frame->payload[16]);
    st->status = frame->payload[18];

    return true;
}

bool arm_decode_target(const arm_frame_t *frame, arm_target_t *tgt)
{
    if ((frame == NULL) || (tgt == NULL)) {
        return false;
    }
    if ((frame->type != ARM_MSG_TARGET) || (frame->len != ARM_LEN_TARGET)) {
        return false;
    }

    for (uint32_t i = 0u; i < (uint32_t)ARM_AXIS_COUNT; ++i) {
        tgt->q_tgt[i] = (int16_t)read_u16_le(&frame->payload[i * 2u]);
    }
    tgt->duration_ms = read_u16_le(&frame->payload[8]);
    tgt->profile     = frame->payload[10];

    /* 未定义的 profile 不丢帧（丢帧会让上位机一直在等），按梯形执行 */
    if (tgt->profile > ARM_PROFILE_SCURVE) {
        tgt->profile = ARM_PROFILE_TRAP;
    }

    return true;
}

/* ---------------- 字节流解析 ---------------- */

static void parser_reset(arm_parser_t *parser)
{
    parser->state = ARM_PARSE_HEAD;
    parser->index = 0u;
    parser->len   = 0u;
}

/** 出错后的复位：若当前字节恰好是帧头，就把它当作新帧的开头，
 *  这样"假帧头把真帧头吃掉"的情况仍能恢复同步（拔插 USB 时必然遇到）。*/
static int parser_fail(arm_parser_t *parser, uint8_t byte)
{
    parser_reset(parser);
    if (byte == ARM_FRAME_HEAD) {
        parser->raw[0] = byte;
        parser->index  = 1u;
        parser->state  = ARM_PARSE_TYPE;
    }
    return -1;
}

void arm_parser_init(arm_parser_t *parser)
{
    if (parser == NULL) {
        return;
    }
    (void)memset(parser, 0, sizeof(*parser));
    parser_reset(parser);
}

int arm_parser_push(arm_parser_t *parser, uint8_t byte, arm_frame_t *out)
{
    if ((parser == NULL) || (out == NULL)) {
        return -1;
    }

    switch (parser->state) {
    case ARM_PARSE_HEAD:
        if (byte == ARM_FRAME_HEAD) {
            parser->raw[0] = byte;
            parser->index  = 1u;
            parser->state  = ARM_PARSE_TYPE;
        }
        /* 非帧头字节直接丢弃：总线上电毛刺、波特率错位都是常态 */
        return 0;

    case ARM_PARSE_TYPE:
        parser->raw[1] = byte;
        parser->index  = 2u;
        parser->state  = ARM_PARSE_LEN;
        return 0;

    case ARM_PARSE_LEN:
        if (byte > ARM_MAX_PAYLOAD) {
            return parser_fail(parser, byte);
        }
        parser->raw[2] = byte;
        parser->len    = byte;
        parser->index  = 3u;
        parser->state  = (byte == 0u) ? ARM_PARSE_CRC : ARM_PARSE_PAYLOAD;
        return 0;

    case ARM_PARSE_PAYLOAD:
        parser->raw[parser->index] = byte;
        parser->index++;
        if (parser->index >= (uint8_t)(ARM_RAW_HEAD_LEN + (uint32_t)parser->len)) {
            parser->state = ARM_PARSE_CRC;
        }
        return 0;

    case ARM_PARSE_CRC: {
        uint32_t body_len = ARM_RAW_HEAD_LEN + (uint32_t)parser->len;
        uint8_t  expect   = crc8_calc(parser->raw, body_len);

        if (expect != byte) {
            /* 丢帧、不回复、不计入超时（任务书 6 节） */
            return parser_fail(parser, byte);
        }

        out->type = parser->raw[1];
        out->len  = parser->len;
        if (parser->len > 0u) {
            (void)memcpy(out->payload, &parser->raw[ARM_RAW_HEAD_LEN], (size_t)parser->len);
        }
        parser_reset(parser);
        return 1;
    }

    default:
        return parser_fail(parser, byte);
    }
}
