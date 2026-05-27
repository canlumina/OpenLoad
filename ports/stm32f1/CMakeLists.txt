# ports/stm32f1/CMakeLists.txt
#
# STM32F1 参考 port. 依赖外层 example 工程提供:
#   - stm32cubemx INTERFACE library (HAL/CMSIS includes + defines)
#   - 全局符号: huart1, hdma_usart1_rx, hspi2 等 (CubeMX usart.c/dma.c/spi.c)

cmake_minimum_required(VERSION 3.22)

option(STM32F1_PORT_ENABLE_ESP8266 "Compile UART2 + ESP8266 AT driver" OFF)

set(STM32F1_PORT_SOURCES
    src/port_sys.c
    src/port_flash_int.c
    src/port_flash_w25q64.c
    src/port_io_uart.c
    src/port_button.c
    src/port_rdp.c
    src/board_init.c
)

if(STM32F1_PORT_ENABLE_ESP8266)
    list(APPEND STM32F1_PORT_SOURCES src/port_uart2.c src/port_io_esp8266.c)
endif()

add_library(openload_port_stm32f1 STATIC ${STM32F1_PORT_SOURCES})

target_include_directories(openload_port_stm32f1 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 链 openload 拿到接口头; 链 stm32cubemx 拿到 HAL 头
target_link_libraries(openload_port_stm32f1 PUBLIC
    openload
    stm32cubemx
)
