/*
 * STM32F1 中断向量处理 - 极简版
 *
 * 与 legacy 区别: USART1 IDLE 中断回调改为 port_uart1_idle_isr()。
 */
#include "main.h"
#include "stm32f1xx_it.h"
#include "stm32f1xx_hal.h"
#include "port_stm32f1.h"

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef  hdma_usart1_rx;
extern DMA_HandleTypeDef  hdma_usart1_tx;

/* ---------- Cortex-M3 内核异常 ---------- */
void NMI_Handler(void)         { while (1) { } }
void HardFault_Handler(void)   { while (1) { } }
void MemManage_Handler(void)   { while (1) { } }
void BusFault_Handler(void)    { while (1) { } }
void UsageFault_Handler(void)  { while (1) { } }
void SVC_Handler(void)         { }
void DebugMon_Handler(void)    { }
void PendSV_Handler(void)      { }
void SysTick_Handler(void)     { HAL_IncTick(); }

/* ---------- 外设中断 ---------- */
void DMA1_Channel4_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_usart1_tx); }
void DMA1_Channel5_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_usart1_rx); }

void USART1_IRQHandler(void)
{
    /* IDLE 中断: 由 port 层把 DMA buffer 残余数据搬入 ringbuf */
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE) != RESET) {
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);
        port_uart1_idle_isr();
    }
    /* HAL 处理 RX-half/RX-complete (DMA 循环模式不必关心 TC) */
    HAL_UART_IRQHandler(&huart1);
}
