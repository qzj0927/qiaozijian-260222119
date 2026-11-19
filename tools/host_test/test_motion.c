/**
 * @file    test_motion.c
 * @brief   映射与插补的 PC 端单元测试
 *
 * 重点验证 T1/T2/T3 的判据在算法层面成立：
 *  - 映射单调、可反算、永不出标定行程；
 *  - 归一化进度曲线单调、C1 连续、端点精确；
 *  - 200Hz 采样下二次差分（≈加速度）不超过配置的加速度上限；
 *  - 多轴共用一条曲线 -> 同拍到位。
 */
#include "motion_profile.h"
#include "servo_map.h"

#include <math.h>
#include <stdio.h>

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

#define EPS 1e-4f

static void test_servo_map(void)
{
    /* 典型 SG90 标定：0.5~2.5ms，90° 对应约 1000µs */
    const servo_map_cfg_t cfg = {
        .pulse_min_us  = 500u,
        .pulse_max_us  = 2500u,
        .zero_us       = 1500u,
        .us_per_deg    = 10.0f,
        .dir           = 1,
        .angle_min_d10 = -900,
        .angle_max_d10 = 900,
        .home_d10      = 0
    };

    (void)printf("[1] 角度->脉宽映射与限位\n");
    CHECK(servo_map_cfg_sane(&cfg), "正常配置被判为非法");
    CHECK(servo_map_angle_to_us(&cfg, 0) == 1500u, "0° 应为 1500µs");
    CHECK(servo_map_angle_to_us(&cfg, 900) == 2400u, "+90° 应为 2400µs");
    CHECK(servo_map_angle_to_us(&cfg, -900) == 600u, "-90° 应为 600µs");
    CHECK(servo_map_angle_to_us(&cfg, 450) == 1950u, "+45° 应为 1950µs");

    /* 越界角度必须被钳位，且不超出标定行程 */
    CHECK(servo_map_angle_to_us(&cfg, 3000) == 2400u, "超上限未钳位");
    CHECK(servo_map_angle_to_us(&cfg, -3000) == 600u, "超下限未钳位");
    CHECK(!servo_map_angle_ok(&cfg, 1000), "越界角度被判定为合法");

    /* 全行程扫描：脉宽必须单调且落在 [500,2500] */
    {
        uint16_t prev = 0u;
        int      mono = 1;
        int      in_range = 1;

        for (int q = -1200; q <= 1200; q += 5) {
            uint16_t us = servo_map_angle_to_us(&cfg, (int16_t)q);

            if (us < 500u || us > 2500u) {
                in_range = 0;
            }
            if (q > -900 && q <= 900 && us < prev) {
                mono = 0;
            }
            prev = us;
        }
        CHECK(in_range, "扫描中出现越界脉宽");
        CHECK(mono, "行程内脉宽不单调");
    }

    /* 反算精度：应优于 0.1° */
    {
        float max_err = 0.0f;

        for (int q = -900; q <= 900; q += 10) {
            uint16_t us = servo_map_angle_to_us(&cfg, (int16_t)q);
            int16_t  back = servo_map_us_to_angle(&cfg, us);
            float    err = fabsf((float)(back - q));

            if (err > max_err) {
                max_err = err;
            }
        }
        CHECK(max_err <= 0.5f, "反算误差过大: %.2f (0.1°)", max_err);
    }

    /* 反装方向 */
    {
        servo_map_cfg_t rev = cfg;
        rev.dir = -1;

        CHECK(servo_map_cfg_sane(&rev), "反装配置被判为非法");
        CHECK(servo_map_angle_to_us(&rev, 900) == 600u, "反装 +90° 应为 600µs");
        CHECK(servo_map_angle_to_us(&rev, -900) == 2400u, "反装 -90° 应为 2400µs");
    }

    /* 非法配置必须被拒绝：软限位端点超出脉宽行程 */
    {
        servo_map_cfg_t bad = cfg;
        bad.angle_max_d10 = 1500;  /* 0° +150° -> 3000µs，超出 2500 */

        CHECK(!servo_map_cfg_sane(&bad), "越行程的限位配置未被拒绝");
        CHECK(servo_map_angle_to_us(&bad, 1500) == 2500u, "越行程角度未被脉宽钳位");
    }
}

static void test_profile_shape(void)
{
    (void)printf("[2] 进度曲线：端点、单调性、C1 连续\n");

    const motion_shape_t shapes[2] = {MOTION_SHAPE_TRAP, MOTION_SHAPE_SCURVE};

    for (int k = 0; k < 2; ++k) {
        motion_plan_t p = motion_plan_make(150.0f, 0.5f, shapes[k], 300.0f, 900.0f);

        CHECK(p.duration_s >= 0.5f - EPS, "shape=%d 时长未被保留", (int)shapes[k]);
        CHECK(fabsf(motion_progress(&p, 0.0f)) <= EPS, "shape=%d s(0)!=0", (int)shapes[k]);
        CHECK(fabsf(motion_progress(&p, p.duration_s) - 1.0f) <= EPS,
              "shape=%d s(T)!=1", (int)shapes[k]);
        CHECK(motion_progress(&p, p.duration_s * 2.0f) == 1.0f, "shape=%d 超时后未钳位为 1",
              (int)shapes[k]);
        CHECK(motion_progress(&p, -1.0f) == 0.0f, "shape=%d 负时间未钳位为 0", (int)shapes[k]);

        /* 峰值速度与加速度不得超过配置 */
        CHECK(p.v_peak_dps <= 300.0f * 1.02f, "shape=%d 峰值速度超限: %.1f", (int)shapes[k],
              (double)p.v_peak_dps);
        CHECK(p.a_peak_dps2 <= 900.0f * 1.02f, "shape=%d 峰值加速度超限: %.1f", (int)shapes[k],
              (double)p.a_peak_dps2);

        /* 单调 + 数值导数与解析导数一致（验证 C1 连续、增量为连续函数） */
        {
            const int n = 2000;
            float     prev = 0.0f;
            int       mono = 1;
            float     max_rate_err = 0.0f;

            for (int i = 0; i <= n; ++i) {
                float t = p.duration_s * (float)i / (float)n;
                float s = motion_progress(&p, t);

                if (s < prev - 1e-6f) {
                    mono = 0;
                }
                prev = s;

                if ((i > 0) && (i < n)) {
                    const float h = p.duration_s * 1e-4f;
                    const float num = (motion_progress(&p, t + h) - motion_progress(&p, t - h))
                                      / (2.0f * h);
                    const float ana = motion_progress_rate(&p, t);
                    float err = fabsf(num - ana);

                    if (err > max_rate_err) {
                        max_rate_err = err;
                    }
                }
            }
            CHECK(mono, "shape=%d 进度非单调", (int)shapes[k]);
            CHECK(max_rate_err < 1e-2f, "shape=%d 解析速度与数值导数偏差 %.4f（C1 断裂？）",
                  (int)shapes[k], (double)max_rate_err);
        }
    }
}

static void test_duration_clamp(void)
{
    (void)printf("[3] 请求时长过短时的自动延长\n");

    /* 150° / 900°/s² 的物理下限约 0.82s，请求 10ms 必须被延长而不是硬冲 */
    motion_plan_t p = motion_plan_make(150.0f, 0.01f, MOTION_SHAPE_TRAP, 300.0f, 900.0f);

    CHECK(p.duration_s > 0.5f, "时长未被延长: %.3f", (double)p.duration_s);
    CHECK(p.v_peak_dps <= 300.0f * 1.02f, "延长后仍超速: %.1f", (double)p.v_peak_dps);
    CHECK(p.a_peak_dps2 <= 900.0f * 1.02f, "延长后仍超加速: %.1f", (double)p.a_peak_dps2);

    /* 位移为 0 时不应规划出运动 */
    motion_plan_t z = motion_plan_make(0.0f, 0.5f, MOTION_SHAPE_TRAP, 300.0f, 900.0f);

    CHECK(z.shape == MOTION_SHAPE_STEP, "零位移未按阶跃处理");
    CHECK(z.duration_s == 0.0f, "零位移时长应为 0");

    /* 阶跃模式：第一拍即到位 */
    CHECK(motion_progress(&z, 0.001f) == 1.0f, "阶跃模式未立即到位");
}

static void test_multiaxis_sync(void)
{
    (void)printf("[4] 多轴同起同停（200Hz 采样模拟）\n");

    const float q0[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float q1[4] = {150.0f, -40.0f, 95.0f, 12.0f};
    const float limit_v = 300.0f;
    const float limit_a = 900.0f;
    const float dt = 0.005f;   /* 200Hz */

    float max_delta = 0.0f;

    for (int i = 0; i < 4; ++i) {
        float d = fabsf(q1[i] - q0[i]);

        if (d > max_delta) {
            max_delta = d;
        }
    }

    const motion_shape_t shapes[2] = {MOTION_SHAPE_TRAP, MOTION_SHAPE_SCURVE};

    for (int k = 0; k < 2; ++k) {
        motion_plan_t p = motion_plan_make(max_delta, 0.0f, shapes[k], limit_v, limit_a);
        float         q[4];
        float         prev[4];
        float         prev_delta[4];
        int           end_tick[4];
        int           moving[4];
        float         max_accel = 0.0f;
        int           monotonic = 1;
        int           i;

        for (i = 0; i < 4; ++i) {
            q[i] = q0[i];
            prev[i] = q0[i];
            prev_delta[i] = 0.0f;
            end_tick[i] = -1;
            moving[i] = 1;
        }

        for (int tick = 0; tick < 4000; ++tick) {
            const float t = (float)tick * dt;
            const float s = motion_progress(&p, t);

            for (i = 0; i < 4; ++i) {
                const float dirn = (q1[i] >= q0[i]) ? 1.0f : -1.0f;

                q[i] = q0[i] + ((q1[i] - q0[i]) * s);

                if (moving[i] != 0) {
                    const float d = q[i] - prev[i];

                    /* 增量的符号必须与运动方向一致：出现反号说明有回退/抖动 */
                    if ((d * dirn) < -1e-5f) {
                        monotonic = 0;
                    }
                    if (tick > 0) {
                        /* 二次差分 / dt² ≈ 加速度，用于检查"增量连续" */
                        const float acc = fabsf(d - prev_delta[i]) / (dt * dt);

                        if (acc > max_accel) {
                            max_accel = acc;
                        }
                    }
                    prev_delta[i] = d;

                    if (fabsf(q[i] - q1[i]) < 1e-4f) {
                        end_tick[i] = tick;
                        moving[i] = 0;
                    }
                }
                prev[i] = q[i];
            }

            if ((moving[0] == 0) && (moving[1] == 0) && (moving[2] == 0) && (moving[3] == 0)) {
                break;
            }
        }

        CHECK(monotonic, "shape=%d 出现反向增量", (int)shapes[k]);
        CHECK(max_accel <= limit_a * 1.10f, "shape=%d 加速度超限: %.0f deg/s²", (int)shapes[k],
              (double)max_accel);

        /* 同拍到位的判据：所有轴结束拍相差不超过 1 拍 */
        {
            int spread = 0;

            for (i = 0; i < 4; ++i) {
                int d = end_tick[i] - end_tick[0];

                if (d < 0) {
                    d = -d;
                }
                if (d > spread) {
                    spread = d;
                }
            }
            CHECK(spread <= 1, "shape=%d 各轴到位时刻相差 %d 拍", (int)shapes[k], spread);
            CHECK(end_tick[0] > 0, "shape=%d 未产生运动", (int)shapes[k]);
        }

        /* 最终位置必须是目标值（无残差） */
        for (i = 0; i < 4; ++i) {
            CHECK(fabsf(q[i] - q1[i]) < 1e-3f, "shape=%d 轴%d 终点残差 %.4f", (int)shapes[k], i,
                  (double)fabsf(q[i] - q1[i]));
        }
    }
}

int main(void)
{
    (void)printf("=== 映射与插补单元测试 ===\n");
    test_servo_map();
    test_profile_shape();
    test_duration_clamp();
    test_multiaxis_sync();
    (void)printf("结果: %d 通过, %d 失败\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
