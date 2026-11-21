/**
 * @file    servo_map.c
 * @brief   角度与脉宽的线性映射
 */
#include "servo_map.h"

#include <math.h>

static void cfg_bounds(const servo_cfg_t *cfg, int16_t *lo, int16_t *hi)
{
    if (cfg->angle_min_d10 <= cfg->angle_max_d10) {
        *lo = cfg->angle_min_d10;
        *hi = cfg->angle_max_d10;
    } else {
        *lo = cfg->angle_max_d10;
        *hi = cfg->angle_min_d10;
    }
}

static uint16_t clamp_u16(uint16_t v, uint16_t lo, uint16_t hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

int16_t servo_map_clamp_angle(const servo_cfg_t *cfg, int16_t q_d10)
{
    int16_t lo;
    int16_t hi;

    if (cfg == NULL) {
        return q_d10;
    }
    cfg_bounds(cfg, &lo, &hi);
    if (q_d10 < lo) {
        return lo;
    }
    if (q_d10 > hi) {
        return hi;
    }
    return q_d10;
}

bool servo_map_angle_ok(const servo_cfg_t *cfg, int16_t q_d10)
{
    int16_t lo;
    int16_t hi;

    if (cfg == NULL) {
        return false;
    }
    cfg_bounds(cfg, &lo, &hi);
    return (q_d10 >= lo) && (q_d10 <= hi);
}

uint16_t servo_map_angle_to_us(const servo_cfg_t *cfg, int16_t q_d10)
{
    int16_t q;
    int8_t  dir;
    float   us;

    if (cfg == NULL) {
        return 0u;
    }

    q   = servo_map_clamp_angle(cfg, q_d10);
    dir = (cfg->dir >= 0) ? (int8_t)1 : (int8_t)-1;

    us = (float)cfg->zero_us + ((float)dir * cfg->us_per_deg * ((float)q * 0.1f));

    if (us <= 0.0f) {
        return cfg->pulse_min_us;
    }
    /* 四舍五入到 1µs：定时器分辨率就是 1µs */
    return clamp_u16((uint16_t)(us + 0.5f), cfg->pulse_min_us, cfg->pulse_max_us);
}

bool servo_map_cfg_sane(const servo_cfg_t *cfg)
{
    int16_t lo;
    int16_t hi;
    float   us_a;
    float   us_b;

    if (cfg == NULL) {
        return false;
    }
    if ((cfg->pulse_min_us == 0u) || (cfg->pulse_min_us >= cfg->pulse_max_us)) {
        return false;
    }
    if (cfg->us_per_deg <= 0.0f) {
        return false;
    }
    if ((cfg->dir != 1) && (cfg->dir != -1)) {
        return false;
    }
    if ((cfg->zero_us < cfg->pulse_min_us) || (cfg->zero_us > cfg->pulse_max_us)) {
        return false;
    }

    cfg_bounds(cfg, &lo, &hi);
    if (lo == hi) {
        return false;
    }

    /* 两个限位端点必须严格落在脉宽范围内（不能贴边），
     * 否则插补到端点时会被脉宽钳位"拉住"，表现为走不到目标角。 */
    us_a = (float)cfg->zero_us + ((float)cfg->dir * cfg->us_per_deg * ((float)lo * 0.1f));
    us_b = (float)cfg->zero_us + ((float)cfg->dir * cfg->us_per_deg * ((float)hi * 0.1f));

    if (fminf(us_a, us_b) <= ((float)cfg->pulse_min_us + 1.0f)) {
        return false;
    }
    if (fmaxf(us_a, us_b) >= ((float)cfg->pulse_max_us - 1.0f)) {
        return false;
    }
    return true;
}
