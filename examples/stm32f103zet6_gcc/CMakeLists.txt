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
    startup_stm32f103xe.s
)

target_include_directories(${PROJECT_NAME} PRIVATE
    Core/Inc
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    openload                          # 框架核心
    openload_port_stm32f1             # STM32F1 port (含 console/flash/sys ops)
    STM32_Drivers                     # HAL/CMSIS .c
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
