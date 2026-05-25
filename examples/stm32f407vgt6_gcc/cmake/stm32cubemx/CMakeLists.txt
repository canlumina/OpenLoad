# ----------------------------------------------------------------------------
#  STM32CubeMX 子构建: 提供 HAL + CMSIS (本地化复制到 ../Drivers/)
#
#  HAL/CMSIS 文件已从 STM32Cube_FW_F4_V1.28.3 复制到 examples/.../Drivers/,
#  不再引用本机的 ~/STM32Cube/Repository — 工程自包含, 任意机器 clone 即编.
# ----------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.22)

enable_language(C ASM)

set(DRIVERS_ROOT "${CMAKE_SOURCE_DIR}/Drivers")
set(HAL_DRV      "${DRIVERS_ROOT}/STM32F4xx_HAL_Driver")
set(CMSIS        "${DRIVERS_ROOT}/CMSIS")

set(STM32_DEFS
    USE_HAL_DRIVER
    STM32F407xx
    $<$<CONFIG:Debug>:DEBUG>
)

set(STM32_INCLUDES
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}                              # for openload_config.h, partitions.def
    ${HAL_DRV}/Inc
    ${HAL_DRV}/Inc/Legacy
    ${CMSIS}/Device/ST/STM32F4xx/Include
    ${CMSIS}/Include
)

set(HAL_SOURCES
    ${HAL_DRV}/Src/stm32f4xx_hal.c
    ${HAL_DRV}/Src/stm32f4xx_hal_rcc.c
    ${HAL_DRV}/Src/stm32f4xx_hal_rcc_ex.c
    ${HAL_DRV}/Src/stm32f4xx_hal_gpio.c
    ${HAL_DRV}/Src/stm32f4xx_hal_dma.c
    ${HAL_DRV}/Src/stm32f4xx_hal_dma_ex.c
    ${HAL_DRV}/Src/stm32f4xx_hal_cortex.c
    ${HAL_DRV}/Src/stm32f4xx_hal_pwr.c
    ${HAL_DRV}/Src/stm32f4xx_hal_pwr_ex.c
    ${HAL_DRV}/Src/stm32f4xx_hal_uart.c
    ${HAL_DRV}/Src/stm32f4xx_hal_spi.c
    ${HAL_DRV}/Src/stm32f4xx_hal_flash.c
    ${HAL_DRV}/Src/stm32f4xx_hal_flash_ex.c
)

# Interface lib: 暴露 includes + defines 给所有依赖者
add_library(stm32cubemx INTERFACE)
target_include_directories(stm32cubemx INTERFACE ${STM32_INCLUDES})
target_compile_definitions(stm32cubemx INTERFACE ${STM32_DEFS})

# Object lib: HAL 源码; 把符号传给主 target 链接
add_library(STM32_Drivers OBJECT ${HAL_SOURCES})
target_link_libraries(STM32_Drivers PUBLIC stm32cubemx)
# ST HAL 内部 -Wunused-parameter 是已知 vendor 警告 (大芯片用到 Banks 字段,
# F407 单 bank 不使用, 但函数签名保留), 局部静音让构建干净.
target_compile_options(STM32_Drivers PRIVATE -Wno-unused-parameter)
