/**
 * @file    test_servo_map.c
 * @brief   T1 映射逻辑的 PC 端验证（gcc 编译，不需要 STM32 工具链）
 *
 * 目的：在没有 ARM 编译器时，至少把"角度->脉宽"的数学验证掉。
 * 编译：gcc -std=c11 -Wall -Wextra -I application -I components \
 *            tools/host_test/test_servo_map.c application/app_config.c \
 *            components/servo_map.c -lm -o build_host/test_servo_map.exe
 */
#include "app_config.h"
#include "servo_map.h"

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

int main(void)
{
    (void)printf("=== servo_map 单元测试 ===\n");

    for (uint32_t i = 0; i < APP_SERVO_COUNT; ++i) {
        const servo_cfg_t *cfg = app_config_servo(i);
        int16_t            lo;
        int16_t            hi;

        CHECK(cfg != NULL, "轴 %u 配置为空", i);
        if (cfg == NULL) {
            continue;
        }
        CHECK(servo_map_cfg_sane(cfg), "轴 %u 配置自检未通过", i);
        CHECK(servo_map_angle_to_us(cfg, 0) == cfg->zero_us,
              "轴 %u 0° 应为 zero_us=%u，实际 %u", i, (unsigned)cfg->zero_us,
              (unsigned)servo_map_angle_to_us(cfg, 0));

        lo = servo_map_clamp_angle(cfg, -30000);
        hi = servo_map_clamp_angle(cfg, 30000);
        CHECK(lo >= cfg->angle_min_d10, "轴 %u 下限钳位错误", i);
        CHECK(hi <= cfg->angle_max_d10, "轴 %u 上限钳位错误", i);

        /* 全行程扫描：脉宽必须落在标定范围内，且方向单调 */
        {
            uint16_t prev = 0u;
            int      mono = 1;
            int      in_range = 1;
            int16_t  first = (cfg->dir >= 0) ? cfg->angle_min_d10 : cfg->angle_max_d10;
            int16_t  last  = (cfg->dir >= 0) ? cfg->angle_max_d10 : cfg->angle_min_d10;
            int16_t  step  = (int16_t)((last >= first) ? 5 : -5);

            for (int16_t q = first; (step > 0) ? (q <= last) : (q >= last); q = (int16_t)(q + step)) {
                const uint16_t us = servo_map_angle_to_us(cfg, q);

                if ((us < cfg->pulse_min_us) || (us > cfg->pulse_max_us)) {
                    in_range = 0;
                }
                if ((prev != 0u) && (us < prev)) {
                    mono = 0;
                }
                prev = q;
            }
            CHECK(in_range, "轴 %u 出现越界脉宽", i);
            (void)mono;   /* 单调性由下面的逐度比较验证 */
        }

        {
            int mono = 1;
            uint16_t prev = servo_map_angle_to_us(cfg, cfg->angle_min_d10);

            for (int16_t q = cfg->angle_min_d10; q <= cfg->angle_max_d10; q = (int16_t)(q + 10)) {
                const uint16_t us = servo_map_angle_to_us(cfg, q);

                if ((cfg->dir >= 0) && (us < prev)) {
                    mono = 0;
                }
                if ((cfg->dir < 0) && (us > prev)) {
                    mono = 0;
                }
                prev = us;
            }
            CHECK(mono, "轴 %u 脉宽随角度不单调（dir=%d）", i, (int)cfg->dir);
        }
    }

    /* 越界目标必须被钳位到限位，而不是输出超程脉宽 */
    {
        const servo_cfg_t *cfg = app_config_servo(0);
        const uint16_t us = servo_map_angle_to_us(cfg, 3000);

        CHECK(us <= cfg->pulse_max_us, "越界角度输出了超程脉宽: %u", (unsigned)us);
        CHECK(us == servo_map_angle_to_us(cfg, cfg->angle_max_d10), "越界角度未钳到上限");
    }

    (void)printf("结果: %d 通过, %d 失败\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
