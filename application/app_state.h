/**
 * @file    app_state.h
 * @brief   四态状态机：手动 / 自动 / 回零 / 故障（T6 选做）
 *
 * 迁移规则（画进设计说明）：
 *
 *   MANUAL --短按--> AUTO --短按--> MANUAL
 *   MANUAL/AUTO --长按--> HOMING --到位--> 回到进入前的模式
 *   任意状态 --0x22 急停 / I2C 保护 / 校验失败--> FAULT
 *   FAULT --0x21 模式 3（或长按）--> MANUAL        （只能用显式复位退出）
 *
 * 明确约束：状态切换过程中不清零 q_cmd，因此不会"甩关节"。
 */
#ifndef APPLICATION_APP_STATE_H
#define APPLICATION_APP_STATE_H

#include <stdint.h>
#include <stdbool.h>

#include "bsp_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_MODE_MANUAL = 0,   /**< 手动：按键操作，接受上位机目标用于调试 */
    APP_MODE_AUTO,         /**< 自动：接受上位机目标 */
    APP_MODE_HOMING,       /**< 回零：运动完成后自动回到进入前的模式 */
    APP_MODE_FAULT         /**< 故障：锁定输出，只能显式复位 */
} app_mode_t;

/** @brief 初始化，默认 MANUAL */
void app_state_init(void);

/** @brief 当前模式 */
app_mode_t app_state_mode(void);

/** @brief 协议 0x21 的请求入口；从 FAULT 只能进 FAULT_RESET */
bool app_state_request(app_mode_t mode);

/** @brief 进入故障态（幂等） */
void app_state_enter_fault(void);

/** @brief 故障态撤销：仅在 FAULT 下有效 */
bool app_state_clear_fault(void);

/** @brief 是否处于故障态 */
bool app_state_is_fault(void);

/**
 * @brief 按键事件处理（在主循环里调，不要在中断里调）
 * @note  内部可能发起回零运动，因此必须运行在非中断上下文
 */
void app_state_on_key(bsp_key_event_t event);

/** @brief 回零运动结束，由 app_main 在检测到运动完成后调用 */
void app_state_on_homing_done(void);

/** @brief 是否正在回零 */
bool app_state_is_homing(void);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_APP_STATE_H */
