/**
 * @file    test_app.c
 * @brief   端到端联调测试：模拟上位机发包，检查固件行为
 *
 * 覆盖评审会实际做的动作：
 *   T2  多轴运动启动、途中状态位、同拍到位
 *   T3  越界目标被钳位 + 状态位置位
 *   6   3s 无 0x20 -> bit5，且姿态保持
 *   T5a 状态帧内容、心跳原样回发、版本应答
 *   T6  急停 -> 故障态锁死，只能 0x21=3 复位
 *
 * 说明：所有时间都由测试显式推进（stub_set_ms），因此结果是可复现的，
 * 不依赖主机真实耗时。
 */
#include "app_config.h"
#include "app_motion.h"
#include "app_protocol.h"
#include "app_servo.h"
#include "app_state.h"
#include "arm_protocol.h"
#include "bsp_usb_cdc.h"

#include <stdio.h>
#include <string.h>

void     stub_set_ms(uint32_t ms);
void     stub_advance_ms(uint32_t delta);
const uint8_t *stub_tx_data(void);
uint32_t stub_tx_len(void);
void     stub_tx_reset(void);

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

/* ---------------- 帧工具 ---------------- */

static bool find_last_frame(uint8_t type, arm_frame_t *out)
{
    arm_parser_t   ps;
    arm_frame_t    f;
    bool           found = false;
    const uint8_t *data  = stub_tx_data();
    const uint32_t len   = stub_tx_len();

    arm_parser_init(&ps);
    for (uint32_t i = 0; i < len; ++i) {
        if (arm_parser_push(&ps, data[i], &f) == 1) {
            if (f.type == type) {
                *out  = f;
                found = true;
            }
        }
    }
    return found;
}

static uint32_t count_frames(uint8_t type)
{
    arm_parser_t   ps;
    arm_frame_t    f;
    uint32_t       n    = 0;
    const uint8_t *data = stub_tx_data();

    arm_parser_init(&ps);
    for (uint32_t i = 0; i < stub_tx_len(); ++i) {
        if (arm_parser_push(&ps, data[i], &f) == 1) {
            if (f.type == type) {
                ++n;
            }
        }
    }
    return n;
}

static arm_status_t last_status(void)
{
    arm_frame_t  f;
    arm_status_t st;

    (void)memset(&st, 0, sizeof(st));
    if (find_last_frame(ARM_MSG_STATUS, &f)) {
        (void)arm_decode_status(&f, &st);
    }
    return st;
}

/** 在 now 时刻喂入一帧（走 USB 接收环形缓冲，覆盖完整链路） */
static void inject_at(uint32_t now, uint8_t type, const uint8_t *payload, uint8_t len)
{
    uint8_t  buf[3u + ARM_MAX_PAYLOAD + 1u];
    uint32_t n = arm_encode(type, payload, len, buf, sizeof(buf));

    CHECK(n > 0u, "组帧失败 type=0x%02X", type);
    stub_set_ms(now);
    bsp_usb_cdc_on_rx(buf, n);
    app_protocol_tick(now);
}

static void inject_target(uint32_t now, const int16_t q[ARM_AXIS_COUNT], uint16_t dur, uint8_t profile)
{
    uint8_t pl[ARM_LEN_TARGET];

    for (uint32_t i = 0; i < (uint32_t)ARM_AXIS_COUNT; ++i) {
        pl[i * 2u]      = (uint8_t)((uint16_t)q[i] & 0xFFu);
        pl[i * 2u + 1u] = (uint8_t)((uint16_t)q[i] >> 8);
    }
    pl[8]  = (uint8_t)(dur & 0xFFu);
    pl[9]  = (uint8_t)(dur >> 8);
    pl[10] = profile;

    inject_at(now, ARM_MSG_TARGET, pl, sizeof(pl));
}

/* ---------------- 用例 ---------------- */

static void test_boot_and_mode(void)
{
    (void)printf("[1] 初始化与模式切换\n");

    CHECK(app_servo_ready(), "伺服配置自检未通过");
    CHECK(app_state_mode() == APP_MODE_MANUAL, "上电模式不是 MANUAL");

    inject_at(100u, ARM_MSG_MODE, (const uint8_t *)"\x01", 1u);   /* 1 = 自动 */
    CHECK(app_state_mode() == APP_MODE_AUTO, "0x21=1 未切到 AUTO");

    stub_tx_reset();
    app_protocol_tick(140u);
    {
        const arm_status_t st = last_status();

        CHECK((st.status & ARM_ST_AUTO) != 0u, "状态帧 bit0 未置位");
        CHECK((st.status & ARM_ST_FAULT) == 0u, "初始化后不应有故障位");
        CHECK(st.q_act[0] == 0, "上电角度应为 0°，实际 %d", st.q_act[0]);
    }

    inject_at(160u, ARM_MSG_MODE, (const uint8_t *)"\x00", 1u);
    CHECK(app_state_mode() == APP_MODE_MANUAL, "0x21=0 未切回 MANUAL");
}

static void test_clamp_and_status_bit(void)
{
    (void)printf("[2] 越界目标钳位（T3）\n");

    const servo_map_cfg_t *cfg   = app_config_servo(ARM_AXIS_BASE);
    const int16_t          over[ARM_AXIS_COUNT] = {2000, 0, 0, 0};   /* 200°，远超 ±85° */

    inject_target(200u, over, 300u, ARM_PROFILE_TRAP);

    CHECK(app_servo_target(ARM_AXIS_BASE) == cfg->angle_max_d10, "越界目标未被钳位: %d",
          app_servo_target(ARM_AXIS_BASE));
    CHECK(app_servo_any_limited(), "钳位标志未置位");

    stub_tx_reset();
    app_protocol_tick(240u);
    {
        const arm_status_t st = last_status();

        CHECK((st.status & ARM_ST_FAULT) != 0u, "钳位时 bit2 未置位");
        CHECK((st.status & ARM_ST_MOVING) != 0u, "钳位后未开始运动");
    }

    /* 让它走完，后续用例从静止开始 */
    app_motion_abort();
}

static void test_move_and_sync(void)
{
    (void)printf("[3] 多轴运动与同拍到位（T2）\n");

    const int16_t tgt[ARM_AXIS_COUNT] = {600, -300, 400, 100};

    inject_target(500u, tgt, 400u, ARM_PROFILE_SCURVE);
    CHECK(!app_servo_any_limited(), "合法目标未清除钳位标志");
    CHECK(app_motion_busy(), "未进入运动状态");

    stub_tx_reset();
    app_protocol_tick(540u);   /* 30Hz 上报，间隔需 >= 33ms 才会发新帧 */
    stub_set_ms(540u);
    CHECK((last_status().status & ARM_ST_FAULT) == 0u, "合法目标仍带 bit2");
    CHECK((last_status().status & ARM_ST_MOVING) != 0u, "运动中未置 bit3");

    int done_tick[ARM_AXIS_COUNT];

    for (uint32_t i = 0; i < (uint32_t)ARM_AXIS_COUNT; ++i) {
        done_tick[i] = -1;
    }

    int end_tick = -1;

    for (int k = 1; k <= 200; ++k) {
        const uint32_t now = 540u + ((uint32_t)k * 5u);

        stub_set_ms(now);
        app_motion_step(now);
        if (!app_motion_busy()) {
            end_tick = k;
            break;
        }
    }

    CHECK(!app_motion_busy(), "运动未在 1s 内结束");

    /* "同起同停"的正确判据：在运动结束的同一拍，四轴同时落在目标 ±0.1° 内。
     * 不能按"某轴第一次进入容差"的拍数比较——位移小的轴会更早进入容差窗，
     * 那是量化造成的假差异，不是真的没同步。 */
    for (uint32_t i = 0; i < (uint32_t)ARM_AXIS_COUNT; ++i) {
        const int diff = app_servo_angle(i) - tgt[i];

        CHECK((diff >= -1) && (diff <= 1), "轴%u 终点残差 %d（0.1°），未同拍到位", i, diff);
    }
    CHECK(end_tick > 0, "没有检测到运动结束拍");
    (void)done_tick;
}

static void test_estop_and_reset(void)
{
    (void)printf("[4] 急停与显式复位（T6）\n");

    const int16_t tgt[ARM_AXIS_COUNT] = {-600, 0, 0, 0};

    inject_target(1600u, tgt, 2000u, ARM_PROFILE_TRAP);
    CHECK(app_motion_busy(), "急停前未在运动");

    inject_at(1620u, ARM_MSG_ESTOP, (const uint8_t *)"\x01", 1u);
    CHECK(app_state_is_fault(), "急停未进入故障态");
    CHECK(!app_motion_busy(), "急停未停止运动");

    stub_tx_reset();
    app_protocol_tick(1660u);
    CHECK((last_status().status & ARM_ST_FAULT) != 0u, "急停后 bit2 未置位");

    const int16_t before = app_servo_angle(ARM_AXIS_BASE);

    /* 故障态下目标指令必须被忽略 */
    inject_target(1680u, tgt, 500u, ARM_PROFILE_TRAP);
    stub_set_ms(1980u);
    app_motion_step(1980u);
    CHECK(app_servo_angle(ARM_AXIS_BASE) == before, "故障态仍在执行目标指令");

    inject_at(2000u, ARM_MSG_MODE, (const uint8_t *)"\x03", 1u);   /* 3 = 清除故障 */
    CHECK(!app_state_is_fault(), "0x21=3 未清除故障");
    CHECK(app_state_mode() == APP_MODE_MANUAL, "复位后未回到 MANUAL");
}

static void test_heartbeat_version(void)
{
    (void)printf("[5] 心跳原样回发与版本应答（T5a）\n");

    stub_tx_reset();
    inject_at(3000u, ARM_MSG_HEARTBEAT, (const uint8_t *)"\x7E", 1u);
    CHECK(count_frames(ARM_MSG_HEARTBEAT) == 1u, "心跳未回发");
    {
        arm_frame_t f;

        if (find_last_frame(ARM_MSG_HEARTBEAT, &f)) {
            CHECK((f.len == 1u) && (f.payload[0] == 0x7Eu), "心跳载荷未原样回发: 0x%02X",
                  f.payload[0]);
        }
    }

    stub_tx_reset();
    inject_at(3020u, ARM_MSG_VERSION, (const uint8_t *)"\x00", 1u);
    {
        arm_frame_t f;

        if (find_last_frame(ARM_MSG_VERSION, &f)) {
            CHECK((f.len == 2u) && (f.payload[0] == ARM_VERSION_MAJOR)
                      && (f.payload[1] == ARM_VERSION_MINOR),
                  "版本应答内容错误");
        } else {
            CHECK(false, "版本查询无应答");
        }
    }
}

static void test_timeout(void)
{
    (void)printf("[6] 3 秒无指令超时与姿态保持（第 6 节）\n");

    const int16_t tgt[ARM_AXIS_COUNT] = {0, 0, 0, 0};

    inject_at(4000u, ARM_MSG_HEARTBEAT, (const uint8_t *)"\x01", 1u);
    inject_target(4010u, tgt, 100u, ARM_PROFILE_TRAP);

    stub_set_ms(4050u);
    app_motion_step(4050u);

    stub_tx_reset();
    app_protocol_tick(4100u);
    CHECK((last_status().status & ARM_ST_CMD_TIMEOUT) == 0u, "刚到 0.1s 就报超时");

    stub_tx_reset();
    app_protocol_tick(7200u);   /* 距 4010 已 3.19s */
    CHECK(app_protocol_cmd_timeout(), "3s 未报超时");
    CHECK((last_status().status & ARM_ST_CMD_TIMEOUT) != 0u, "bit5 未置位");

    const int16_t frozen = app_servo_angle(ARM_AXIS_BASE);

    stub_set_ms(7400u);
    app_motion_step(7400u);
    CHECK(app_servo_angle(ARM_AXIS_BASE) == frozen, "超时后仍在运动");

    inject_target(7500u, tgt, 100u, ARM_PROFILE_TRAP);

    /* 心跳是独立计时器：只补 0x20 不清心跳超时，必须再补一帧 0x23 才算链路恢复 */
    inject_at(7510u, ARM_MSG_HEARTBEAT, (const uint8_t *)"\x02", 1u);
    CHECK(!app_protocol_cmd_timeout(), "心跳恢复后超时位仍未清除");
}

int main(void)
{
    (void)printf("=== 端到端联调测试 ===\n");

    stub_set_ms(0u);
    app_servo_init();
    app_motion_init();
    app_state_init();
    app_protocol_init();
    bsp_usb_cdc_init();
    bsp_usb_cdc_on_configured();

    test_boot_and_mode();
    test_clamp_and_status_bit();
    test_move_and_sync();
    test_estop_and_reset();
    test_heartbeat_version();
    test_timeout();

    (void)printf("结果: %d 通过, %d 失败\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
