# ============================================================================
#  OpenLoad - STM32F407VGT6 GCC 示例工程 (DevEBox MCUDev 板)
#
#  构建:
#    cmake --preset=Debug
#    cmake --build build/Debug
#
#  输出: build/Debug/openload_bootloader.{elf,bin,map}
# ============================================================================
cmake_minimum_required(VERSION 3.22)

project(openload_bootloader LANGUAGES C ASM)

# ---------- 引入框架核心 + STM32F4 port ----------
# M2: YMODEM + HTTP OTA
set(OPENLOAD_ENABLE_YMODEM   ON CACHE BOOL "Enable YMODEM receiver"   FORCE)
set(OPENLOAD_ENABLE_HTTP_OTA ON CACHE BOOL "Enable HTTP OTA receiver" FORCE)
# M2-13: 持久化操作日志
set(OPENLOAD_ENABLE_OPLOG    ON CACHE BOOL "Enable persistent op log" FORCE)
# M3-4: AES-128-CTR image decryption
set(OPENLOAD_ENABLE_AES_128_CTR ON CACHE BOOL "Enable AES-128-CTR" FORCE)
# M4-1: SHA-256
set(OPENLOAD_ENABLE_SHA256      ON CACHE BOOL "Enable SHA-256"     FORCE)
# M4-2: Ed25519 签名
set(OPENLOAD_ENABLE_ED25519     ON CACHE BOOL "Enable Ed25519"     FORCE)
# ESP8266 (UART2) 启用. 同步在 openload_config.h 也置 1.
set(STM32F4_PORT_ENABLE_ESP8266 ON CACHE BOOL "Enable UART2 + ESP8266 port" FORCE)

add_subdirectory(${CMAKE_SOURCE_DIR}/../../openload    ${CMAKE_BINARY_DIR}/_openload)
add_subdirectory(${CMAKE_SOURCE_DIR}/../../ports/stm32f4
                                                       ${CMAKE_BINARY_DIR}/_port_stm32f4)

# ---------- STM32CubeMX HAL + CMSIS + startup ----------
add_subdirectory(cmake/stm32cubemx)

# ---------- 把用户工程源加入 openload 框架的 include 路径 ----------
target_include_directories(openload PUBLIC ${CMAKE_SOURCE_DIR})

# ---------- 主 elf 目标 ----------
add_executable(${PROJECT_NAME}
    Core/Src/main.c
    Core/Src/dma.c
    Core/Src/gpio.c
    Core/Src/usart.c
    Core/Src/stm32f4xx_it.c
    Core/Src/stm32f4xx_hal_msp.c
    Core/Src/syscalls.c
    Core/Src/system_stm32f4xx.c
    partitions.c
    commands.c
    wifi_cfg.c
    startup_stm32f407xx.s
)

target_include_directories(${PROJECT_NAME} PRIVATE
    Core/Inc
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    # --whole-archive: 强制把 openload + port 静态库内的所有 .o 拉进 elf,
    # 否则 OL_*_REGISTER 所在 TU 因 "无外部引用" 被 linker GC, 链接段为空,
    # 运行时找不到设备/命令/receiver.
    "-Wl,--whole-archive"
    openload
    openload_port_stm32f4
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
