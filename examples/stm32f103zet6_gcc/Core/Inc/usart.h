/*
 * usart.h - 极简版, 仅声明 UART1 公开符号
 */
#pragma once

#include "main.h"

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef  hdma_usart1_rx;
extern DMA_HandleTypeDef  hdma_usart1_tx;

void MX_USART1_UART_Init(void);
