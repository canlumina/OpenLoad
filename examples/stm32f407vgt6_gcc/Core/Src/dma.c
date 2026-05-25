/*
 * DMA 控制器初始化 + IRQ enable (F4 用 Stream + Channel, 不是 F1 的 Channel)
 *
 *   USART1 RX = DMA2 Stream2 Channel4   (console)
 *   USART1 TX = DMA2 Stream7 Channel4   (保留, 阻塞模式 M1 阶段未走 DMA TX)
 *   USART2 RX = DMA1 Stream5 Channel4   (ESP8266, OPENLOAD_ENABLE_ESP8266)
 *   USART2 TX = DMA1 Stream6 Channel4   (ESP8266, 同上)
 */
#include "dma.h"
#include "openload/config.h"

void MX_DMA_Init(void)
{
    __HAL_RCC_DMA2_CLK_ENABLE();
    HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 4, 0);   /* USART1 RX */
    HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);
    HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 4, 0);   /* USART1 TX */
    HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);

#if OPENLOAD_ENABLE_ESP8266
    __HAL_RCC_DMA1_CLK_ENABLE();
    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 4, 0);   /* USART2 RX */
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 4, 0);   /* USART2 TX */
    HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
#endif
}
