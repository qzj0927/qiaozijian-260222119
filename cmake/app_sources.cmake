# ---------------------------------------------------------------------------
# 新增源码登记（任务书 2.3 第 3 条：新增 .c 必须登记到根目录 CMakeLists.txt 的
# target_sources() 与 target_include_directories() 中）
#
# 用法：在 CubeMX 生成的根 CMakeLists.txt 里，找到 target_sources(${CMAKE_PROJECT_NAME} ...)
# 之后、target_include_directories(...) 之后，加一行：
#
#     include(${CMAKE_CURRENT_LIST_DIR}/cmake/app_sources.cmake)
#
# 这样 CubeMX 重新生成工程时，只有这一行需要手工补回，其余生成内容不动。
# 注意：不要修改 cmake/stm32cubemx/CMakeLists.txt（任务书 2.4 第 5 条禁止）。
# ---------------------------------------------------------------------------

set(APP_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

set(APP_SOURCES
    ${APP_ROOT}/protocol/crc8.c
    ${APP_ROOT}/protocol/arm_protocol.c
    ${APP_ROOT}/components/servo_map.c
    ${APP_ROOT}/components/motion_profile.c
    ${APP_ROOT}/components/mpu6050.c
    ${APP_ROOT}/bsp/bsp_time.c
    ${APP_ROOT}/bsp/bsp_sys.c
    ${APP_ROOT}/bsp/bsp_gpio.c
    ${APP_ROOT}/bsp/bsp_pwm.c
    ${APP_ROOT}/bsp/bsp_i2c.c
    ${APP_ROOT}/bsp/bsp_usart.c
    ${APP_ROOT}/bsp/usb_link.c
    ${APP_ROOT}/application/app_config.c
    ${APP_ROOT}/application/app_servo.c
    ${APP_ROOT}/application/app_motion.c
    ${APP_ROOT}/application/app_state.c
    ${APP_ROOT}/application/app_protocol.c
    ${APP_ROOT}/application/app_main.c
)

# 传输层二选一：默认 CDC（T5a 必做），-DUSB_LINK_HID=ON 切到 Custom HID（T5b）
# 两个实现共用同一套 usb_link 接口，应用层与协议层代码不需要任何改动。
option(USB_LINK_HID "Build with Custom HID transport instead of CDC" OFF)

if(USB_LINK_HID)
    list(APPEND APP_SOURCES ${APP_ROOT}/bsp/bsp_usb_hid.c)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE USB_LINK_HID=1)
else()
    list(APPEND APP_SOURCES ${APP_ROOT}/bsp/bsp_usb_cdc.c)
endif()

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${APP_SOURCES})

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${APP_ROOT}/protocol
    ${APP_ROOT}/components
    ${APP_ROOT}/bsp
    ${APP_ROOT}/application
)

# 任务书 2.2 构建要求：打开 -Wall -Wextra，并鼓励处理到零警告
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -Wextra)

# 数学库（atan2f / sqrtf / lroundf）
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE m)
