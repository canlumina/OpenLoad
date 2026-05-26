/*
 * UART1 (console) + UART2 (ESP8266 AT) 配置 — F4 版本
 *
 * F4 与 F1 关键差异:
 *   - GPIO 必须显式设 Alternate = GPIO_AF7_USART1 / GPIO_AF7_USART2
 *   - DMA 用 Stream + Channel: USART1 走 DMA2, USART2 走 DMA1
 *   - DMA_InitTypeDef 多 .Channel/.FIFOMode/.FIFOThreshold/.MemBurst/.PeriphBurst
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

        /* PA9 TX, PA10 RX, AF7 */
        gi.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
        gi.Mode      = GPIO_MODE_AF_PP;
        gi.Pull      = GPIO_PULLUP;
        gi.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        gi.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &gi);

        /* DMA RX: DMA2 Stream2 Channel4, 循环 */
        hdma_usart1_rx.Instance                 = DMA2_Stream2;
        hdma_usart1_rx.Init.Channel             = DMA_CHANNEL_4;
        hdma_usart1_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        hdma_usart1_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_usart1_rx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_usart1_rx.Init.Mode                = DMA_CIRCULAR;
        hdma_usart1_rx.Init.Priority            = DMA_PRIORITY_LOW;
        hdma_usart1_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK) { Error_Handler(); }
        __HAL_LINKDMA(uartHandle, hdmarx, hdma_usart1_rx);

        /* DMA TX: DMA2 Stream7 Channel4, 普通 (保留, M1 走阻塞) */
        hdma_usart1_tx.Instance                 = DMA2_Stream7;
        hdma_usart1_tx.Init.Channel             = DMA_CHANNEL_4;
        hdma_usart1_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
        hdma_usart1_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_usart1_tx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_usart1_tx.Init.Mode                = DMA_NORMAL;
        hdma_usart1_tx.Init.Priority            = DMA_PRIORITY_LOW;
        hdma_usart1_tx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK) { Error_Handler(); }
        __HAL_LINKDMA(uartHandle, hdmatx, hdma_usart1_tx);

        HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);
    }
#if OPENLOAD_ENABLE_ESP8266
    else if (uartHandle->Instance == USART2) {
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA2 TX, PA3 RX, AF7 */
        gi.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
        gi.Mode      = GPIO_MODE_AF_PP;
        gi.Pull      = GPIO_PULLUP;
        gi.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        gi.Alternate = GPIO_AF7_USART2;
        HAL_GPIO_Init(GPIOA, &gi);

        /* DMA RX: DMA1 Stream5 Channel4, 循环 */
        hdma_usart2_rx.Instance                 = DMA1_Stream5;
        hdma_usart2_rx.Init.Channel             = DMA_CHANNEL_4;
        hdma_usart2_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        hdma_usart2_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_usart2_rx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_usart2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart2_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_usart2_rx.Init.Mode                = DMA_CIRCULAR;
        hdma_usart2_rx.Init.Priority            = DMA_PRIORITY_LOW;
        hdma_usart2_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(&hdma_usart2_rx) != HAL_OK) { Error_Handler(); }
        __HAL_LINKDMA(uartHandle, hdmarx, hdma_usart2_rx);

        /* DMA TX: DMA1 Stream6 Channel4, 普通 (保留) */
        hdma_usart2_tx.Instance                 = DMA1_Stream6;
        hdma_usart2_tx.Init.Channel             = DMA_CHANNEL_4;
        hdma_usart2_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
        hdma_usart2_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_usart2_tx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_usart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart2_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_usart2_tx.Init.Mode                = DMA_NORMAL;
        hdma_usart2_tx.Init.Priority            = DMA_PRIORITY_LOW;
        hdma_usart2_tx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
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
