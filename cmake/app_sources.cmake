# ---------------------------------------------------------------------------
# 用户代码登记（任务书 2.3 第 3 条：新增 .c 必须登记到 target_sources）
#
# 由根 CMakeLists.txt 末尾 include 进来：
#     include(${CMAKE_CURRENT_LIST_DIR}/cmake/app_sources.cmake)
#
# 每加一个 .c 文件，就在下面追加一行（不要用 file(GLOB)，
# 评审要能在 CMakeLists 里直接看到工程包含哪些源文件）。
# ---------------------------------------------------------------------------

set(APP_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # --- application/ 任务与行为层 ---
    ${APP_ROOT}/application/app_main.c

    # --- bsp/ 板级驱动 ---
    ${APP_ROOT}/bsp/bsp_gpio.c
    ${APP_ROOT}/bsp/bsp_usart.c
    ${APP_ROOT}/bsp/bsp_sys.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${APP_ROOT}/application
    ${APP_ROOT}/bsp
)

# 任务书 2.2 构建要求：打开 -Wall -Wextra，鼓励处理到零警告
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -Wextra)
