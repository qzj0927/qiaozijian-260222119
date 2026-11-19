/**
 * @file    app_main.h
 * @brief   应用入口：init / 1ms 中断 / 主循环
 *
 * 与 CubeMX 生成代码的接缝只有三处（见 docs/01-CubeMX配置清单.md）：
 *   1. main() 的 USER CODE 2   -> app_init();
 *   2. while(1) 的 USER CODE 3 -> app_loop();
 *   3. stm32f4xx_it.c / 回调里 -> app_tick_1ms();
 *
 * 除此之外不改动任何生成代码，CubeMX 重新生成不会覆盖业务逻辑。
 */
#ifndef APPLICATION_APP_MAIN_H
#define APPLICATION_APP_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 外设初始化完成后调用：自检、标定、回零、打印开机信息 */
void app_init(void);

/** @brief 1ms 定时中断里调用（TIM6），只做计数与置标志，不做耗时操作 */
void app_tick_1ms(void);

/** @brief 主循环，不会返回 */
void app_loop(void);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_APP_MAIN_H */
