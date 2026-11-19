/**
 * @file    app_state.c
 * @brief   四态状态机实现
 */
#include "app_state.h"

#include "app_motion.h"
#include "arm_protocol.h"

static app_mode_t s_mode               = APP_MODE_MANUAL;
static app_mode_t s_mode_before_homing = APP_MODE_MANUAL;

void app_state_init(void)
{
    s_mode               = APP_MODE_MANUAL;
    s_mode_before_homing = APP_MODE_MANUAL;
}

app_mode_t app_state_mode(void)
{
    return s_mode;
}

void app_state_enter_fault(void)
{
    if (s_mode == APP_MODE_FAULT) {
        return;
    }
    /* 先停运动再改状态：反过来会有一拍按旧目标继续输出 */
    app_motion_abort();
    s_mode = APP_MODE_FAULT;
}

bool app_state_clear_fault(void)
{
    if (s_mode != APP_MODE_FAULT) {
        return false;
    }
    s_mode = APP_MODE_MANUAL;
    return true;
}

bool app_state_is_fault(void)
{
    return (s_mode == APP_MODE_FAULT);
}

bool app_state_request(app_mode_t mode)
{
    switch (mode) {
    case APP_MODE_MANUAL:
    case APP_MODE_AUTO:
        if (s_mode == APP_MODE_FAULT) {
            return false;   /* 故障态只能复位，不能直接切模式 */
        }
        s_mode = mode;
        return true;

    case APP_MODE_HOMING:
        if (s_mode == APP_MODE_FAULT) {
            return false;
        }
        s_mode_before_homing = s_mode;
        if (app_motion_home() != 0) {
            return false;
        }
        s_mode = APP_MODE_HOMING;
        return true;

    default:
        /* 协议里 3 = 清除故障，不走这里 */
        return false;
    }
}

void app_state_on_key(bsp_key_event_t event)
{
    if (event == BSP_KEY_LONG) {
        if (s_mode == APP_MODE_FAULT) {
            (void)app_state_clear_fault();
        } else {
            (void)app_state_request(APP_MODE_HOMING);
        }
        return;
    }

    if (event == BSP_KEY_SHORT) {
        if (s_mode == APP_MODE_FAULT) {
            return;   /* 故障态短按无效，防止误触恢复 */
        }
        if (s_mode == APP_MODE_AUTO) {
            s_mode = APP_MODE_MANUAL;
        } else {
            s_mode = APP_MODE_AUTO;
        }
    }
}

void app_state_on_homing_done(void)
{
    if (s_mode == APP_MODE_HOMING) {
        s_mode = (s_mode_before_homing == APP_MODE_HOMING) ? APP_MODE_MANUAL : s_mode_before_homing;
    }
}

bool app_state_is_homing(void)
{
    return (s_mode == APP_MODE_HOMING);
}
