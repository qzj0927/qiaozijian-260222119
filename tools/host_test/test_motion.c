/**
 * @file    test_motion.c
 * @brief   T2 插补的 PC 端验证（不需要 STM32 工具链）
 *
 * 覆盖：端点、单调、C1 连续（解析导数 vs 数值导数）、限速限加速、
 *       请求时长过短时自动延长、200Hz 多轴同拍到位。
 */
#include "motion_profile.h"

#include <math.h>
#include <stdio.h>

static int g_fail;
static int g_pass;

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

#define LIM_V 240.0f
#define LIM_A 720.0f

static void test_shape(motion_shape_t shape)
{
    const motion_plan_t p = motion_plan_make(150.0f, 0.5f, shape, LIM_V, LIM_A);

    CHECK(p.duration_s >= 0.5f - 1e-4f, "shape=%d 请求时长未被保留", (int)shape);
    CHECK(fabsf(motion_progress(&p, 0.0f)) <= 1e-4f, "shape=%d s(0)!=0", (int)shape);
    CHECK(fabsf(motion_progress(&p, p.duration_s) - 1.0f) <= 1e-4f, "shape=%d s(T)!=1",
          (int)shape);
    CHECK(motion_progress(&p, p.duration_s * 2.0f) == 1.0f, "shape=%d 超时未钳位", (int)shape);
    CHECK(motion_progress(&p, -1.0f) == 0.0f, "shape=%d 负时间未钳位", (int)shape);

    CHECK(p.v_peak_dps <= LIM_V * 1.02f, "shape=%d 峰值速度超限 %.1f", (int)shape,
          (double)p.v_peak_dps);
    CHECK(p.a_peak_dps2 <= LIM_A * 1.02f, "shape=%d 峰值加速度超限 %.1f", (int)shape,
          (double)p.a_peak_dps2);

    /* 单调 + 解析导数与数值导数一致 */
    {
        const int n = 2000;
        float     prev = 0.0f;
        int       mono = 1;
        float     max_rate_err = 0.0f;

        for (int i = 0; i <= n; ++i) {
            const float t = p.duration_s * (float)i / (float)n;
            const float s = motion_progress(&p, t);

            if (s < prev - 1e-6f) {
                mono = 0;
            }
            prev = s;

            if ((i > 0) && (i < n)) {
                const float h = p.duration_s * 1e-4f;
                const float num = (motion_progress(&p, t + h) - motion_progress(&p, t - h))
                                  / (2.0f * h);
                const float ana = motion_progress_rate(&p, t);
                const float err = fabsf(num - ana);

                if (err > max_rate_err) {
                    max_rate_err = err;
                }
            }
        }
        CHECK(mono, "shape=%d 进度非单调", (int)shape);
        CHECK(max_rate_err < 1e-2f, "shape=%d 解析速度与数值导数偏差 %.4f", (int)shape,
              (double)max_rate_err);
    }
}

static void test_duration_clamp(void)
{
    const motion_plan_t p = motion_plan_make(150.0f, 0.01f, MOTION_SHAPE_TRAP, LIM_V, LIM_A);

    CHECK(p.duration_s > 0.5f, "时长未被延长: %.3f", (double)p.duration_s);
    CHECK(p.v_peak_dps <= LIM_V * 1.02f, "延长后仍超速: %.1f", (double)p.v_peak_dps);
    CHECK(p.a_peak_dps2 <= LIM_A * 1.02f, "延长后仍超加速: %.1f", (double)p.a_peak_dps2);

    /* req=0 表示按最大速度，不应变成阶跃 */
    {
        const motion_plan_t q = motion_plan_make(90.0f, 0.0f, MOTION_SHAPE_TRAP, LIM_V, LIM_A);

        CHECK(q.shape == MOTION_SHAPE_TRAP, "req=0 被误判为阶跃");
        CHECK(q.duration_s > 0.0f, "req=0 时长为 0");
    }

    /* 零位移：一拍到位 */
    {
        const motion_plan_t z = motion_plan_make(0.0f, 0.5f, MOTION_SHAPE_TRAP, LIM_V, LIM_A);

        CHECK(z.shape == MOTION_SHAPE_STEP, "零位移未按阶跃处理");
        CHECK(z.duration_s == 0.0f, "零位移时长应为 0");
    }
}

static void test_multiaxis_200hz(void)
{
    const float delta[4] = {150.0f, -40.0f, 95.0f, 12.0f};
    const float dt = 0.005f;   /* 200Hz */

    for (int k = 0; k < 2; ++k) {
        const motion_shape_t shape = (k == 0) ? MOTION_SHAPE_TRAP : MOTION_SHAPE_SCURVE;
        const motion_plan_t  p = motion_plan_make(150.0f, 0.0f, shape, LIM_V, LIM_A);
        float   q[4];
        float   prev_q[4];
        float   prev_d[4];
        int     end_tick = -1;
        int     moving[4];
        float   max_acc = 0.0f;
        int     mono = 1;

        for (int i = 0; i < 4; ++i) {
            q[i] = 0.0f;
            prev_q[i] = 0.0f;
            prev_d[i] = 0.0f;
            moving[i] = 1;
        }

        for (int tick = 0; tick < 2000; ++tick) {
            const float s = motion_progress(&p, (float)tick * dt);

            if ((s >= 1.0f) && (end_tick < 0)) {
                end_tick = tick;
            }

            for (int i = 0; i < 4; ++i) {
                const float dirn = (delta[i] >= 0.0f) ? 1.0f : -1.0f;

                q[i] = delta[i] * s;

                if (moving[i] != 0) {
                    const float d = q[i] - prev_q[i];

                    if ((d * dirn) < -1e-5f) {
                        mono = 0;
                    }
                    if (tick > 0) {
                        const float acc = fabsf(d - prev_d[i]) / (dt * dt);

                        if (acc > max_acc) {
                            max_acc = acc;
                        }
                    }
                    prev_d[i] = d;
                    prev_q[i] = q[i];

                    if (s >= 1.0f) {
                        moving[i] = 0;
                    }
                }
            }

            if (end_tick >= 0) {
                break;
            }
        }

        CHECK(mono, "shape=%d 出现反向增量", (int)shape);
        CHECK(max_acc <= LIM_A * 1.10f, "shape=%d 加速度超限 %.0f", (int)shape, (double)max_acc);
        CHECK(end_tick > 0, "shape=%d 没有检测到结束拍", (int)shape);

        /* 同拍判据：在 s 到 1 的同一拍，四轴必须同时落在目标上。
         * 不能按"某轴首次进入容差"比较——小位移轴会更早进入，那是量化差异。 */
        for (int i = 0; i < 4; ++i) {
            CHECK(fabsf(q[i] - delta[i]) < 1e-3f, "shape=%d 轴%d 终点残差 %.4f", (int)shape, i,
                  (double)fabsf(q[i] - delta[i]));
        }
    }
}

int main(void)
{
    (void)printf("=== motion_profile 单元测试 ===\n");
    (void)printf("[1] 梯形 / S 曲线形状与连续性\n");
    test_shape(MOTION_SHAPE_TRAP);
    test_shape(MOTION_SHAPE_SCURVE);
    (void)printf("[2] 时长与边界条件\n");
    test_duration_clamp();
    (void)printf("[3] 200Hz 多轴同起同停\n");
    test_multiaxis_200hz();
    (void)printf("结果: %d 通过, %d 失败\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
