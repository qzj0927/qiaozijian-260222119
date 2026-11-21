/**
 * @file    app_main.h
 * @brief   应用入口（T0 阶段：自检打印 + LED 1Hz + 按键翻转）
 *
 * 与 CubeMX 生成代码只有三处接缝（见 docs/00 执行手册）：
 *   1. main() 里 MX_xxx_Init() 之后   -> app_init();
 *   2. while(1) 里                    -> app_loop();
 *   3. TIM2 更新中断                   -> app_tick_1ms();
 * 其余都写在 application/ 与 bsp/ 里，CubeMX 重新生成不会覆盖。
 */
#ifndef APP_MAIN_H
#define APP_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 外设初始化完成后调用：初始化 LED/串口、打印自检信息 */
void app_init(void);

/** @brief 1ms 定时中断里调用（TIM2），只做计数与置标志 */
void app_tick_1ms(void);

/** @brief 主循环，不返回 */
void app_loop(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MAIN_H */
