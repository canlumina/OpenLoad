/*
 * DMA1 controller 初始化 + IRQ enable
 *
 * 使用情况:
 *   Ch4 = USART1 TX  (port_io_uart, 保留备用 — M2 阶段仍走阻塞)
 *   Ch5 = USART1 RX  (port_io_uart 循环模式)
 *   Ch6 = USART2 RX  (port_uart2 循环模式,    OPENLOAD_ENABLE_ESP8266)
 *   Ch7 = USART2 TX  (port_uart2 保留备用,    OPENLOAD_ENABLE_ESP8266)
 */
#include "dma.h"
#include "openload/config.h"

void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 4, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 4, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);

#if OPENLOAD_ENABLE_ESP8266
    HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 4, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 4, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);
#endif
}
