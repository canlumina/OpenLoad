/*
 * usart.h - UART1 (console) + UART2 (ESP8266) 公开符号
 */
#pragma once

#include "main.h"
#include "openload/config.h"

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef  hdma_usart1_rx;
extern DMA_HandleTypeDef  hdma_usart1_tx;

void MX_USART1_UART_Init(void);

#if OPENLOAD_ENABLE_ESP8266
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef  hdma_usart2_rx;
extern DMA_HandleTypeDef  hdma_usart2_tx;

void MX_USART2_UART_Init(void);
#endif
