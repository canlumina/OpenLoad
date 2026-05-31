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

# ---------- OpenLoad Kconfig 集成 ----------
# 配置存于 .config (Kconfig 格式), 用 tools/menuconfig.py 编辑.
# 默认 ${CMAKE_CURRENT_SOURCE_DIR}/.config, 可 -DOPENLOAD_KCONFIG_FILE=...
# override (CI minimal 走 ci_minimal/.config).
set(OPENLOAD_KCONFIG_FILE ".config"
    CACHE FILEPATH "Path to OpenLoad Kconfig .config file (relative or absolute)")

if(NOT IS_ABSOLUTE "${OPENLOAD_KCONFIG_FILE}")
    set(OPENLOAD_KCONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${OPENLOAD_KCONFIG_FILE}")
endif()

if(NOT EXISTS "${OPENLOAD_KCONFIG_FILE}")
    message(FATAL_ERROR
        "OpenLoad .config not found: ${OPENLOAD_KCONFIG_FILE}\n"
        "示例 .config 随仓库提供; 若丢失, 从版本库恢复:\n"
        "  git checkout -- examples/stm32f407vgt6_gcc/.config\n"
        "或用 menuconfig 新建一份:\n"
        "  python tools/menuconfig.py examples/stm32f407vgt6_gcc/.config")
endif()

# .config 变化触发 reconfigure
set_property(DIRECTORY APPEND PROPERTY
    CMAKE_CONFIGURE_DEPENDS "${OPENLOAD_KCONFIG_FILE}")

# 跑 genconfig.py 出 autoconfig.h + kconfig.cmake
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(KCFG_OUT "${CMAKE_CURRENT_BINARY_DIR}/kconfig")
file(MAKE_DIRECTORY "${KCFG_OUT}")

execute_process(
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/../../tools/genconfig.py"
            "${OPENLOAD_KCONFIG_FILE}"
            --out-dir "${KCFG_OUT}"
    RESULT_VARIABLE _kcfg_ret
    OUTPUT_VARIABLE _kcfg_out
    ERROR_VARIABLE  _kcfg_err
)
if(NOT _kcfg_ret EQUAL 0)
    message(FATAL_ERROR
        "tools/genconfig.py failed (rc=${_kcfg_ret}):\n"
        "stdout: ${_kcfg_out}\n"
        "stderr: ${_kcfg_err}")
endif()
message(STATUS "OpenLoad Kconfig: ${OPENLOAD_KCONFIG_FILE}")

# 把 OPENLOAD_ENABLE_* cache vars + STM32F4_PORT_ENABLE_ESP8266 设上
include("${KCFG_OUT}/openload_kconfig.cmake")

# ---------- 引入框架核心 + STM32F4 port ----------
add_subdirectory(${CMAKE_SOURCE_DIR}/../../openload    ${CMAKE_BINARY_DIR}/_openload)
add_subdirectory(${CMAKE_SOURCE_DIR}/../../ports/stm32f4
                                                       ${CMAKE_BINARY_DIR}/_port_stm32f4)

# ---------- STM32CubeMX HAL + CMSIS + startup ----------
add_subdirectory(cmake/stm32cubemx)

# ---------- 把用户工程源加入 openload 框架的 include 路径 ----------
# openload_config.h shim (CMAKE_SOURCE_DIR) → openload_autoconfig.h (KCFG_OUT)
# PUBLIC: openload 库自己 + 链接到它的 exe target 都拿到 KCFG_OUT
target_include_directories(openload PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${KCFG_OUT}
)

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
    ${CMAKE_SOURCE_DIR}/../common/partitions.c
    ${CMAKE_SOURCE_DIR}/../common/commands.c
    ${CMAKE_SOURCE_DIR}/../common/wifi_cfg.c
    startup_stm32f407xx.s
)

target_include_directories(${PROJECT_NAME} PRIVATE
    Core/Inc
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/../common
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
