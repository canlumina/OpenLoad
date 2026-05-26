/*
 * UART1 (console, M1) + UART2 (ESP8266 AT, M2) 配置
 *
 * 两个 USART 共用一个 HAL_UART_MspInit/MspDeInit, 内部按 instance 分发.
 * MspInit 顺序对 GPIO/DMA/NVIC 配置敏感, 改动前先看 STM32CubeMX 生成模板.
 */
#include "usart.h"

UART_HandleTypeDef huart1;
DMA_HandleTypeDef  hdma_usart1_rx;
DMA_HandleTypeDef  hdma_usart1_tx;

#if OPENLOAD_ENABLE_ESP8266
UART_HandleTypeDef huart2;
DMA_HandleTypeDef  hdma_usart2_rx;
DMA_HandleTypeDef  hdma_usart2_tx;
#endif

void MX_USART1_UART_Init(void)
{
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}

#if OPENLOAD_ENABLE_ESP8266
void MX_USART2_UART_Init(void)
{
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = OPENLOAD_ESP_UART_BAUD;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
}
#endif

void HAL_UART_MspInit(UART_HandleTypeDef *uartHandle)
{
    GPIO_InitTypeDef gi = {0};

    if (uartHandle->Instance == USART1) {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA9 TX */
        gi.Pin   = GPIO_PIN_9;
        gi.Mode  = GPIO_MODE_AF_PP;
        gi.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &gi);

        /* PA10 RX */
        gi.Pin  = GPIO_PIN_10;
        gi.Mode = GPIO_MODE_INPUT;
        gi.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(GPIOA, &gi);

        /* DMA RX: DMA1 Channel5, 循环 */
        hdma_usart1_rx.Instance                 = DMA1_Channel5;
        hdma_usart1_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        hdma_usart1_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_usart1_rx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_usart1_rx.Init.Mode                = DMA_CIRCULAR;
        hdma_usart1_rx.Init.Priority            = DMA_PRIORITY_LOW;
        if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK) { Error_Handler(); }
        __HAL_LINKDMA(uartHandle, hdmarx, hdma_usart1_rx);

        /* DMA TX: DMA1 Channel4, 普通模式 */
        hdma_usart1_tx.Instance                 = DMA1_Channel4;
        hdma_usart1_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
        hdma_usart1_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_usart1_tx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_usart1_tx.Init.Mode                = DMA_NORMAL;
        hdma_usart1_tx.Init.Priority            = DMA_PRIORITY_LOW;
        if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK) { Error_Handler(); }
        __HAL_LINKDMA(uartHandle, hdmatx, hdma_usart1_tx);

        HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);
    }
#if OPENLOAD_ENABLE_ESP8266
    else if (uartHandle->Instance == USART2) {
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA2 TX (AF_PP) / PA3 RX (上拉输入) */
        gi.Pin   = GPIO_PIN_2;
        gi.Mode  = GPIO_MODE_AF_PP;
        gi.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &gi);

        gi.Pin  = GPIO_PIN_3;
        gi.Mode = GPIO_MODE_INPUT;
        gi.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(GPIOA, &gi);

        /* DMA RX: DMA1 Channel6, 循环 */
        hdma_usart2_rx.Instance                 = DMA1_Channel6;
        hdma_usart2_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        hdma_usart2_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_usart2_rx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_usart2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart2_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_usart2_rx.Init.Mode                = DMA_CIRCULAR;
        hdma_usart2_rx.Init.Priority            = DMA_PRIORITY_LOW;
        if (HAL_DMA_Init(&hdma_usart2_rx) != HAL_OK) { Error_Handler(); }
        __HAL_LINKDMA(uartHandle, hdmarx, hdma_usart2_rx);

        /* DMA TX: DMA1 Channel7, 普通 (M2 阶段 TX 仍走阻塞, 保留以便后续切换) */
        hdma_usart2_tx.Instance                 = DMA1_Channel7;
        hdma_usart2_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
        hdma_usart2_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_usart2_tx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_usart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart2_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_usart2_tx.Init.Mode                = DMA_NORMAL;
        hdma_usart2_tx.Init.Priority            = DMA_PRIORITY_LOW;
        if (HAL_DMA_Init(&hdma_usart2_tx) != HAL_OK) { Error_Handler(); }
        __HAL_LINKDMA(uartHandle, hdmatx, hdma_usart2_tx);

        HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USART2_IRQn);
    }
#endif
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *uartHandle)
{
    if (uartHandle->Instance == USART1) {
        __HAL_RCC_USART1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
        if (uartHandle->hdmarx) { HAL_DMA_DeInit(uartHandle->hdmarx); }
        if (uartHandle->hdmatx) { HAL_DMA_DeInit(uartHandle->hdmatx); }
        HAL_NVIC_DisableIRQ(USART1_IRQn);
    }
#if OPENLOAD_ENABLE_ESP8266
    else if (uartHandle->Instance == USART2) {
        __HAL_RCC_USART2_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2 | GPIO_PIN_3);
        if (uartHandle->hdmarx) { HAL_DMA_DeInit(uartHandle->hdmarx); }
        if (uartHandle->hdmatx) { HAL_DMA_DeInit(uartHandle->hdmatx); }
        HAL_NVIC_DisableIRQ(USART2_IRQn);
    }
#endif
}
