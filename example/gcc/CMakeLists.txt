cmake_minimum_required(VERSION 3.20)

project(OpenLoad C ASM)

# Options
option(BUILD_OPENLOAD_EXAMPLE "Build STM32F103 example target" ON)
set(MCU_FAMILY STM32F1 CACHE STRING "MCU family")
set(MCU_MODEL STM32F103xE CACHE STRING "MCU model define for HAL")

# Toolchain note: pass -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_LIST_DIR}/cmake/arm-gcc-toolchain.cmake when using arm-none-eabi-gcc

if(NOT CMAKE_TOOLCHAIN_FILE)
  message(STATUS "No CMAKE_TOOLCHAIN_FILE set. For cross-compiling use ${CMAKE_CURRENT_LIST_DIR}/cmake/arm-gcc-toolchain.cmake")
endif()

# Common compile options similar to Keil project
set(COMMON_DEFINES
  USE_HAL_DRIVER
  ${MCU_MODEL}
)

set(COMMON_C_FLAGS
  -mcpu=cortex-m3
  -mthumb
  -ffunction-sections
  -fdata-sections
  -fno-builtin
  -fno-common
  -Wall
  -Wextra
  -Wno-unused-parameter
  -Wno-missing-field-initializers
  -std=c11
)

set(COMMON_ASM_FLAGS
  -mcpu=cortex-m3
  -mthumb
)

set(COMMON_LINK_FLAGS
  -mcpu=cortex-m3
  -mthumb
  -Wl,--gc-sections
  -Wl,--print-memory-usage
  -specs=nosys.specs
  -specs=nano.specs
)

if(BUILD_OPENLOAD_EXAMPLE)
  enable_language(ASM)

  # Sources
  file(GLOB_RECURSE FAL_SOURCES
    ../../src/fal/src/*.c
  )
  file(GLOB_RECURSE EASYFLASH_SOURCES
    ../../src/easyflash/src/*.c
  )
  file(GLOB_RECURSE SFUD_SOURCES
    ../../src/SUFD/src/*.c
  )

  set(USER_SOURCES
    ./User/main.c
    ./User/stm32f1xx_it.c
    ./User/bootloader.c
    ./User/xmodem.c
    ./User/ymodem.c
    ./User/syscalls.c
    ./User/Utils/crc32.c
    ../platform/stm32/CMSIS/Device/ST/STM32F1xx/Source/Templates/system_stm32f1xx.c
    ../platform/stm32/CMSIS/Device/ST/STM32F1xx/Source/Templates/gcc/startup_stm32f103xe.S
  )

  # Minimal HAL sources used in Keil project (extend if needed)
  set(HAL_SOURCES
    ../platform/stm32/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c
    ../platform/stm32/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c
    ../platform/stm32/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c
    ../platform/stm32/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c
    ../platform/stm32/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c
    ../platform/stm32/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c
    ../platform/stm32/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_spi.c
    ../platform/stm32/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_sram.c
    ../platform/stm32/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c
    ../platform/stm32/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim_ex.c
    ../platform/stm32/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c
    ../platform/stm32/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_usart.c
    ../platform/stm32/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c
    ../platform/stm32/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_fsmc.c
  )

  set(BSP_SOURCES
    ./Drivers/BSP/bsp_key.c
    ./Drivers/BSP/bsp_led.c
    ./Drivers/BSP/bsp_spi.c
    ./Drivers/SYSTEM/delay/delay.c
    ./Drivers/SYSTEM/sys/sys.c
    ./Drivers/SYSTEM/usart/usart.c
  )

  add_executable(openload.elf
    ${USER_SOURCES}
    ${HAL_SOURCES}
    ${BSP_SOURCES}
    ${FAL_SOURCES}
    ${EASYFLASH_SOURCES}
    ${SFUD_SOURCES}
  )

  target_compile_definitions(openload.elf PRIVATE ${COMMON_DEFINES})
  target_compile_options(openload.elf PRIVATE 
    $<$<COMPILE_LANGUAGE:C>:${COMMON_C_FLAGS}>
    $<$<COMPILE_LANGUAGE:ASM>:${COMMON_ASM_FLAGS}>
  )

  target_include_directories(openload.elf PRIVATE
    ../platform/stm32/CMSIS/Device/ST/STM32F1xx/Include
    ../platform/stm32/STM32F1xx_HAL_Driver/Inc
    ../platform/stm32/CMSIS/Include
    ./User
    ./Drivers/SYSTEM/delay
    ./Drivers/SYSTEM/sys
    ./Drivers/SYSTEM/usart
    ./Drivers/BSP
    ./User/Utils
    ../../src/fal/inc
    ../../src/easyflash/inc
    ../../src/SUFD/inc
  )

  # Linker script
  set(LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/linker/stm32f103xe.ld)
  target_link_options(openload.elf PRIVATE ${COMMON_LINK_FLAGS} -T${LINKER_SCRIPT} -Wl,-Map=${CMAKE_BINARY_DIR}/openload.map)

  # Output formats
  add_custom_command(TARGET openload.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:openload.elf> ${CMAKE_BINARY_DIR}/openload.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:openload.elf> ${CMAKE_BINARY_DIR}/openload.bin
    COMMENT "Generating HEX and BIN files")

endif()
