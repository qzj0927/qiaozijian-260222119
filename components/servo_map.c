/**
 * @file    servo_map.c
 * @brief   角度与脉宽的线性映射
 */
#include "servo_map.h"

#include <math.h>

#define SERVO_MAP_DIR_MIN (-1)
#define SERVO_MAP_DIR_MAX (1)

static uint16_t clamp_us(uint16_t value, uint16_t lo, uint16_t hi)
{
    if (value < lo) {
        return lo;
    }
    if (value > hi) {
        return hi;
    }
    return value;
}

static void cfg_bounds(const servo_map_cfg_t *cfg, int16_t *lo, int16_t *hi)
{
    if (cfg->angle_min_d10 <= cfg->angle_max_d10) {
        *lo = cfg->angle_min_d10;
        *hi = cfg->angle_max_d10;
    } else {
        *lo = cfg->angle_max_d10;
        *hi = cfg->angle_min_d10;
    }
}

int16_t servo_map_clamp_angle(const servo_map_cfg_t *cfg, int16_t q_d10)
{
    int16_t lo;
    int16_t hi;

    if (cfg == 0) {
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

bool servo_map_angle_ok(const servo_map_cfg_t *cfg, int16_t q_d10)
{
    int16_t lo;
    int16_t hi;

    if (cfg == 0) {
        return false;
    }
    cfg_bounds(cfg, &lo, &hi);
    return (q_d10 >= lo) && (q_d10 <= hi);
}

uint16_t servo_map_angle_to_us(const servo_map_cfg_t *cfg, int16_t q_d10)
{
    int16_t  q;
    int8_t   dir;
    float    us;

    if (cfg == 0) {
        return 0u;
    }

    q = servo_map_clamp_angle(cfg, q_d10);
    dir = (cfg->dir >= 0) ? (int8_t)1 : (int8_t)-1;

    /* 用 float 做中间运算：整数插补时 0.1° 的分辨率无法用整数精确表示 */
    us = (float)cfg->zero_us + ((float)dir * cfg->us_per_deg * ((float)q * 0.1f));

    if (us <= 0.0f) {
        return cfg->pulse_min_us;
    }
    if (us > 65535.0f) {
        return cfg->pulse_max_us;
    }

    /* 四舍五入到 1µs：定时器分辨率就是 1µs，多留小数没有意义 */
    return clamp_us((uint16_t)(us + 0.5f), cfg->pulse_min_us, cfg->pulse_max_us);
}

int16_t servo_map_us_to_angle(const servo_map_cfg_t *cfg, uint16_t us)
{
    float q;

    if ((cfg == 0) || (cfg->us_per_deg <= 0.0f)) {
        return 0;
    }
    q = ((float)us - (float)cfg->zero_us) / ((float)cfg->dir * cfg->us_per_deg) * 10.0f;

    if (q > 32767.0f) {
        return 32767;
    }
    if (q < -32768.0f) {
        return -32768;
    }
    return (int16_t)(q >= 0.0f ? (q + 0.5f) : (q - 0.5f));
}

bool servo_map_cfg_sane(const servo_map_cfg_t *cfg)
{
    int16_t lo;
    int16_t hi;
    float   us_a;
    float   us_b;

    if (cfg == 0) {
        return false;
    }
    if ((cfg->pulse_min_us == 0u) || (cfg->pulse_min_us >= cfg->pulse_max_us)) {
        return false;
    }
    if ((cfg->dir < SERVO_MAP_DIR_MIN) || (cfg->dir > SERVO_MAP_DIR_MAX)) {
        return false;
    }
    if (cfg->us_per_deg <= 0.0f) {
        return false;
    }

    cfg_bounds(cfg, &lo, &hi);
    if (lo == hi) {
        return false;
    }
    if ((cfg->zero_us < cfg->pulse_min_us) || (cfg->zero_us > cfg->pulse_max_us)) {
        return false;
    }

    /* 两个软限位端点都必须严格落在脉宽范围内（不能贴边）：
     * 若端点被脉宽钳位"拉住"，关节就走不到目标角，验收里的
     * "重复定位误差不大于 1°" 会在行程两端超标。留 1µs 余量。 */
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
