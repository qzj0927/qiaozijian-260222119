# ---------------------------------------------------------------------------
# 用户代码登记（任务书 2.3 第 3 条：新增 .c 必须登记到 target_sources）
#
# 由根 CMakeLists.txt 末尾 include：
#     include(${CMAKE_CURRENT_LIST_DIR}/cmake/app_sources.cmake)
#
# 每加一个 .c 就在下面追加一行（不用 file(GLOB)，评审要能直接看到源文件列表）。
# ---------------------------------------------------------------------------

set(APP_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # --- application/ 任务与行为层 ---
    ${APP_ROOT}/application/app_config.c
    ${APP_ROOT}/application/app_servo.c
    ${APP_ROOT}/application/app_motion.c
    ${APP_ROOT}/application/app_main.c

    # --- components/ 可复用组件 ---
    ${APP_ROOT}/components/servo_map.c
    ${APP_ROOT}/components/motion_profile.c

    # --- bsp/ 板级驱动 ---
    ${APP_ROOT}/bsp/bsp_gpio.c
    ${APP_ROOT}/bsp/bsp_pwm.c
    ${APP_ROOT}/bsp/bsp_usart.c
    ${APP_ROOT}/bsp/bsp_sys.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${APP_ROOT}/application
    ${APP_ROOT}/components
    ${APP_ROOT}/bsp
)

# 任务书 2.2：打开 -Wall -Wextra，鼓励处理到零警告
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -Wextra)

# 数学库（fminf / fmaxf）
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE m)
