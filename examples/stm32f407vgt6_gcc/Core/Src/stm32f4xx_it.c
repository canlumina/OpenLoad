/*
 * STM32F4 中断向量处理 (Cortex-M4F)
 *
 * UART1 = console.  UART2 = ESP8266 (OPENLOAD_ENABLE_ESP8266).
 * RX*CpltCallback 在 HAL 内部由 HAL_UART_IRQHandler 触发, 实现在 port 层,
 * 见 ports/stm32f4/src/port_io_uart.c (统一 dispatcher).
 */
#include "main.h"
#include "stm32f4xx_it.h"
#include "stm32f4xx_hal.h"
#include "openload/config.h"
#include "port_stm32f4.h"

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef  hdma_usart1_rx;
extern DMA_HandleTypeDef  hdma_usart1_tx;

#if OPENLOAD_ENABLE_ESP8266
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef  hdma_usart2_rx;
extern DMA_HandleTypeDef  hdma_usart2_tx;
#endif

/* ---------- Cortex-M4F 内核异常 ---------- */
void NMI_Handler(void)         { while (1) { } }
void HardFault_Handler(void)   { while (1) { } }
void MemManage_Handler(void)   { while (1) { } }
void BusFault_Handler(void)    { while (1) { } }
void UsageFault_Handler(void)  { while (1) { } }
void SVC_Handler(void)         { }
void DebugMon_Handler(void)    { }
void PendSV_Handler(void)      { }
void SysTick_Handler(void)     { HAL_IncTick(); }

/* ---------- DMA Stream ISR ---------- */
void DMA2_Stream2_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_usart1_rx); }
void DMA2_Stream7_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_usart1_tx); }

#if OPENLOAD_ENABLE_ESP8266
void DMA1_Stream5_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_usart2_rx); }
void DMA1_Stream6_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_usart2_tx); }
#endif

/* ---------- USART ---------- */
void USART1_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE) != RESET) {
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);
        port_uart1_idle_isr();
    }
    HAL_UART_IRQHandler(&huart1);
}

#if OPENLOAD_ENABLE_ESP8266
void USART2_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_IDLE) != RESET) {
        __HAL_UART_CLEAR_IDLEFLAG(&huart2);
        port_uart2_idle_isr();
    }
    HAL_UART_IRQHandler(&huart2);
}
#endif
