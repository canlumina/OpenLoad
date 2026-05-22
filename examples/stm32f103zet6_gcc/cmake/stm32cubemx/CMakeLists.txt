# ----------------------------------------------------------------------------
#  STM32CubeMX 子构建: 提供 HAL + CMSIS + startup
#
#  HAL 源码与 CMSIS 头文件目前直接引用 legacy/example/gcc/Drivers/。
#  TODO (M2): 待 legacy/ 删除时, 把 Drivers 复制到 ports/stm32f1/cubemx_f1/
# ----------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.22)

enable_language(C ASM)

# legacy 中 HAL 库的绝对路径
set(LEGACY_GCC_ROOT "${CMAKE_SOURCE_DIR}/../../legacy/example/gcc")
set(HAL_DRV         "${LEGACY_GCC_ROOT}/Drivers/STM32F1xx_HAL_Driver")
set(CMSIS           "${LEGACY_GCC_ROOT}/Drivers/CMSIS")

set(STM32_DEFS
    USE_HAL_DRIVER
    STM32F103xE
    $<$<CONFIG:Debug>:DEBUG>
)

set(STM32_INCLUDES
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}                              # for openload_config.h, partitions.def
    ${HAL_DRV}/Inc
    ${HAL_DRV}/Inc/Legacy
    ${CMSIS}/Device/ST/STM32F1xx/Include
    ${CMSIS}/Include
)

set(HAL_SOURCES
    ${HAL_DRV}/Src/stm32f1xx_hal.c
    ${HAL_DRV}/Src/stm32f1xx_hal_rcc.c
    ${HAL_DRV}/Src/stm32f1xx_hal_rcc_ex.c
    ${HAL_DRV}/Src/stm32f1xx_hal_gpio.c
    ${HAL_DRV}/Src/stm32f1xx_hal_gpio_ex.c
    ${HAL_DRV}/Src/stm32f1xx_hal_dma.c
    ${HAL_DRV}/Src/stm32f1xx_hal_cortex.c
    ${HAL_DRV}/Src/stm32f1xx_hal_pwr.c
    ${HAL_DRV}/Src/stm32f1xx_hal_uart.c
    ${HAL_DRV}/Src/stm32f1xx_hal_spi.c
    ${HAL_DRV}/Src/stm32f1xx_hal_flash.c
    ${HAL_DRV}/Src/stm32f1xx_hal_flash_ex.c
)

# Interface lib: 暴露 includes + defines 给所有依赖者
add_library(stm32cubemx INTERFACE)
target_include_directories(stm32cubemx INTERFACE ${STM32_INCLUDES})
target_compile_definitions(stm32cubemx INTERFACE ${STM32_DEFS})

# Object lib: HAL 源码; 把符号传给主 target 链接
add_library(STM32_Drivers OBJECT ${HAL_SOURCES})
target_link_libraries(STM32_Drivers PUBLIC stm32cubemx)
