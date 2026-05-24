# ============================================================================
#  OpenLoad - STM32F103ZET6 GCC 示例工程
#
#  构建:
#    cmake --preset=Debug
#    cmake --build build/Debug
#
#  输出: build/Debug/openload.elf / openload.bin / openload.map
# ============================================================================
cmake_minimum_required(VERSION 3.22)

project(openload_bootloader LANGUAGES C ASM)

# ---------- 引入框架核心 + STM32F1 port ----------
# 注: openload/CMakeLists.txt 中的 OPENLOAD_ENABLE_* 选项默认值已对齐 M1
# M2: 把 YMODEM 编入 (option() 不会覆盖已存在的 cache, 用 FORCE 显式覆盖)
set(OPENLOAD_ENABLE_YMODEM ON CACHE BOOL "Enable YMODEM receiver" FORCE)
# M2-12: 启用 ports/stm32f1 的 ESP8266 (UART2) 支持. 同步在 openload_config.h
# 也把 OPENLOAD_ENABLE_ESP8266 置 1 — port_io_uart 的 dispatcher / it.c / dma.c
# 都看的是 C 宏, CMake 这边只控编入.
set(STM32F1_PORT_ENABLE_ESP8266 ON CACHE BOOL "Enable UART2 + ESP8266 port" FORCE)

add_subdirectory(${CMAKE_SOURCE_DIR}/../../openload    ${CMAKE_BINARY_DIR}/_openload)
add_subdirectory(${CMAKE_SOURCE_DIR}/../../ports/stm32f1
                                                       ${CMAKE_BINARY_DIR}/_port_stm32f1)

# ---------- STM32CubeMX HAL + CMSIS + startup ----------
add_subdirectory(cmake/stm32cubemx)

# ---------- 把用户工程源加入 openload 框架的 include 路径 ----------
# (config.h 通过 __has_include 找到 openload_config.h)
target_include_directories(openload PUBLIC ${CMAKE_SOURCE_DIR})

# ---------- 主 elf 目标 ----------
add_executable(${PROJECT_NAME}
    Core/Src/main.c
    Core/Src/dma.c
    Core/Src/gpio.c
    Core/Src/usart.c
    Core/Src/stm32f1xx_it.c
    Core/Src/stm32f1xx_hal_msp.c
    Core/Src/syscalls.c
    Core/Src/system_stm32f1xx.c
    partitions.c
    commands.c
    wifi_cfg.c
    startup_stm32f103xe.s
)

target_include_directories(${PROJECT_NAME} PRIVATE
    Core/Inc
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    # --whole-archive: 强制把 openload + port 静态库内的所有 .o 拉进 elf,
    # 否则 OL_FLASH_DEV_REGISTER / OL_IO_DEV_REGISTER / OL_CMD_REGISTER /
    # OL_RECEIVER_REGISTER 所在的 TU 因 "无外部引用" 被 linker GC, 导致
    # `.ol_flash_devs` 等链接段为空, 运行时找不到设备。
    "-Wl,--whole-archive"
    openload                          # 框架核心 (builtins / xmodem 等)
    openload_port_stm32f1             # STM32F1 port (含 console/flash/sys ops)
    "-Wl,--no-whole-archive"
    STM32_Drivers                     # HAL/CMSIS .c (普通链, 按需拉)
)

# ---------- 链接脚本与 linker flags ----------
set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/linker.ld")
target_link_options(${PROJECT_NAME} PRIVATE
    -T${LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map
)
set_target_properties(${PROJECT_NAME} PROPERTIES
    LINK_DEPENDS ${LINKER_SCRIPT}
    ADDITIONAL_CLEAN_FILES "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map"
)

# ---------- 输出 .bin + 体积报告 ----------
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary -S
            $<TARGET_FILE:${PROJECT_NAME}>
            ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "Generate openload.bin + size report"
)
