/**
 * @file    test_protocol.c
 * @brief   协议层 PC 端单元测试（gcc 直接编译，不需要硬件）
 *
 * 第 2 组用例是任务书第 12 页"示例帧"的逐字节对照：
 *   只要本测试通过，就说明 CRC 参数、字段偏移、字节序、载荷长度都和评审用的
 *   上位机一致，硬件侧不需要再猜协议。
 */
#include "arm_protocol.h"
#include "crc8.h"

#include <stdio.h>
#include <string.h>

static int g_fail = 0;
static int g_pass = 0;

#define CHECK(cond, fmt, ...)                                              \
    do {                                                                   \
        if (cond) {                                                        \
            ++g_pass;                                                      \
        } else {                                                           \
            ++g_fail;                                                      \
            (void)printf("  [FAIL] %s:%d  " fmt "\n", __FILE__, __LINE__, \
                         ##__VA_ARGS__);                                   \
        }                                                                  \
    } while (0)

static const uint8_t k_status_frame[] = {
    0xA5, 0x10, 0x13,
    0x84, 0x03, 0x78, 0x05, 0xB0, 0x04, 0x2C, 0x01,  /* 实际 90.0/140.0/120.0/30.0 */
    0x84, 0x03, 0x78, 0x05, 0xB0, 0x04, 0x2C, 0x01,  /* 目标同实际 */
    0x7B, 0x00,                                      /* 俯仰 12.3° */
    0x01,                                            /* 状态 自动 */
    0xA8                                             /* CRC */
};

static const uint8_t k_target_frame[] = {
    0xA5, 0x20, 0x0B,
    0x84, 0x03, 0x78, 0x05, 0xB0, 0x04, 0x2C, 0x01,
    0xF4, 0x01,   /* 500 ms */
    0x02,         /* S 曲线 */
    0xAB
};

static const uint8_t k_mode_frame[]  = {0xA5, 0x21, 0x01, 0x02, 0x82};
static const uint8_t k_estop_frame[] = {0xA5, 0x22, 0x01, 0x01, 0x36};
static const uint8_t k_ver_frame[]   = {0xA5, 0x24, 0x02, 0x01, 0x00, 0x4B};

/* 喂入一段字节流，返回收到的合法帧数；bad 返回 -1 的次数 */
static int feed(arm_parser_t *ps, const uint8_t *buf, size_t n, arm_frame_t *last, int *bad)
{
    int frames = 0;
    for (size_t i = 0; i < n; ++i) {
        int r = arm_parser_push(ps, buf[i], last);
        if (r == 1) {
            ++frames;
        } else if (r < 0) {
            ++(*bad);
        }
    }
    return frames;
}

static void test_crc(void)
{
    const uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    (void)printf("[1] CRC8 标准校验向量\n");
    CHECK(crc8_calc(check, sizeof(check)) == 0xF4u, "crc8(\"123456789\") != 0xF4");
    CHECK(crc8_calc(NULL, 0) == 0x00u, "空数据应返回 0");
}

static void test_example_frames(void)
{
    arm_parser_t ps;
    int          bad = 0;

    (void)printf("[2] 任务书第 12 页示例帧逐字节对照\n");

    /* ---- 0x10 状态上报 ---- */
    arm_parser_init(&ps);
    arm_frame_t last;
    CHECK(feed(&ps, k_status_frame, sizeof(k_status_frame), &last, &bad) == 1, "状态帧未被识别");
    CHECK(bad == 0, "状态帧被判为非法");

    arm_status_t st;
    (void)memset(&st, 0, sizeof(st));
    CHECK(arm_decode_status(&last, &st), "状态帧解码失败");
    CHECK(st.q_act[0] == 900 && st.q_act[1] == 1400 && st.q_act[2] == 1200 && st.q_act[3] == 300,
          "实际角度解码错误: %d %d %d %d", st.q_act[0], st.q_act[1], st.q_act[2], st.q_act[3]);
    CHECK(st.q_tgt[0] == 900 && st.q_tgt[3] == 300, "目标角度解码错误");
    CHECK(st.pitch == 123, "俯仰角解码错误: %d", st.pitch);
    CHECK(st.status == ARM_ST_AUTO, "状态位解码错误: 0x%02X", st.status);

    uint8_t out[64];
    uint32_t n = arm_encode_status(&st, out, sizeof(out));
    CHECK(n == sizeof(k_status_frame), "状态帧长度 %u != %u", (unsigned)n,
          (unsigned)sizeof(k_status_frame));
    CHECK(memcmp(out, k_status_frame, sizeof(k_status_frame)) == 0, "状态帧重新编码后字节不一致");

    /* ---- 0x20 目标角度 ---- */
    arm_parser_init(&ps);
    bad = 0;
    CHECK(feed(&ps, k_target_frame, sizeof(k_target_frame), &last, &bad) == 1, "目标帧未被识别");

    arm_target_t tgt;
    (void)memset(&tgt, 0, sizeof(tgt));
    CHECK(arm_decode_target(&last, &tgt), "目标帧解码失败");
    CHECK(tgt.q_tgt[1] == 1400, "目标角度错误");
    CHECK(tgt.duration_ms == 500, "duration 错误: %u", tgt.duration_ms);
    CHECK(tgt.profile == ARM_PROFILE_SCURVE, "profile 错误: %u", tgt.profile);

    uint8_t payload[11] = {0x84, 0x03, 0x78, 0x05, 0xB0, 0x04,
                           0x2C, 0x01, 0xF4, 0x01, 0x02};
    n = arm_encode(ARM_MSG_TARGET, payload, sizeof(payload), out, sizeof(out));
    CHECK(n == sizeof(k_target_frame), "目标帧长度不符");
    CHECK(memcmp(out, k_target_frame, sizeof(k_target_frame)) == 0, "目标帧重新编码后字节不一致");

    /* ---- 0x21 / 0x22 / 0x24 ---- */
    /* 注意：任务书第 12 页把这帧标注为"切换到自动"，但载荷是 0x02。
     * 按 4.3 定义表 0x02 = 回零，与 CRC 0x82 一致，判定为任务书笔误。
     * 设备侧一律以定义表为准（1 = 自动），示例帧只用于校验 CRC/字段偏移。*/
    arm_parser_init(&ps);
    bad = 0;
    CHECK(feed(&ps, k_mode_frame, sizeof(k_mode_frame), &last, &bad) == 1, "模式帧未被识别");
    CHECK(last.payload[0] == ARM_MODE_HOMING, "模式值错误");

    arm_parser_init(&ps);
    bad = 0;
    CHECK(feed(&ps, k_estop_frame, sizeof(k_estop_frame), &last, &bad) == 1, "急停帧未被识别");
    CHECK(last.payload[0] == 1u, "急停载荷错误");

    arm_parser_init(&ps);
    bad = 0;
    CHECK(feed(&ps, k_ver_frame, sizeof(k_ver_frame), &last, &bad) == 1, "版本帧未被识别");
    CHECK(last.len == ARM_LEN_VERSION, "版本载荷长度错误");

    n = arm_encode_version(out, sizeof(out));
    CHECK(n == sizeof(k_ver_frame), "版本帧长度不符");
    CHECK(memcmp(out, k_ver_frame, sizeof(k_ver_frame)) == 0, "版本帧重新编码后字节不一致");
}

static void test_robustness(void)
{
    arm_parser_t ps;
    arm_frame_t  frame;
    uint8_t      buf[64];
    int          bad = 0;

    (void)printf("[3] 抗干扰与错误处理\n");

    /* 垃圾前缀后仍能同步：假帧头(0xA5 0x11 0xA5)会把真帧头当成长度丢掉，
     * 解析器必须能在下一次 0xA5 处重新对齐 */
    arm_parser_init(&ps);
    buf[0] = 0x00;
    buf[1] = 0xFF;
    buf[2] = 0xA5;
    buf[3] = 0x11;
    (void)memcpy(&buf[4], k_mode_frame, sizeof(k_mode_frame));
    int frames = 0;
    bad = 0;
    for (size_t i = 0; i < 4u + sizeof(k_mode_frame); ++i) {
        int r = arm_parser_push(&ps, buf[i], &frame);
        if (r == 1) {
            ++frames;
        } else if (r < 0) {
            ++bad;
        }
    }
    CHECK(frames == 1, "垃圾前缀后未能重新同步（收到 %d 帧）", frames);

    /* CRC 错一字节 -> 丢弃 */
    arm_parser_init(&ps);
    (void)memcpy(buf, k_status_frame, sizeof(k_status_frame));
    buf[sizeof(k_status_frame) - 1] ^= 0x01u;
    bad = 0;
    (void)feed(&ps, buf, sizeof(k_status_frame), &frame, &bad);
    CHECK(bad == 1, "CRC 错误未被丢弃");

    /* 长度字段越界 -> 丢弃，不越权写缓冲区 */
    arm_parser_init(&ps);
    buf[0] = 0xA5;
    buf[1] = 0x10;
    buf[2] = 0x7F;
    bad = 0;
    (void)feed(&ps, buf, 3, &frame, &bad);
    CHECK(bad == 1, "超长长度字段未被拒绝");

    /* 载荷里出现 0xA5 不能打断同步 */
    arm_parser_init(&ps);
    uint8_t pl[ARM_LEN_STATUS];
    (void)memset(pl, 0xA5, sizeof(pl));
    uint32_t n = arm_encode(ARM_MSG_STATUS, pl, (uint8_t)sizeof(pl), buf, sizeof(buf));
    CHECK(n == sizeof(k_status_frame), "载荷含 0xA5 时组帧失败");
    frames = 0;
    for (uint32_t i = 0; i < n; ++i) {
        if (arm_parser_push(&ps, buf[i], &frame) == 1) {
            ++frames;
        }
    }
    CHECK(frames == 1, "载荷含 0xA5 时切包错误");

    /* 缓冲区不足必须返回 0 而不是越界写 */
    CHECK(arm_encode(ARM_MSG_MODE, (const uint8_t *)"\x01", 1, buf, 4) == 0u, "缓冲区不足未拒绝");

    /* 增量解析：帧被拆成任意碎片也应还原（模拟 USB 分包） */
    arm_parser_init(&ps);
    frames = 0;
    for (size_t i = 0; i < sizeof(k_status_frame); ++i) {
        if (arm_parser_push(&ps, k_status_frame[i], &frame) == 1) {
            ++frames;
        }
    }
    CHECK(frames == 1, "逐字节喂入未能还原帧");
}

int main(void)
{
    (void)printf("=== 协议层单元测试 ===\n");
    test_crc();
    test_example_frames();
    test_robustness();
    (void)printf("结果: %d 通过, %d 失败\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
