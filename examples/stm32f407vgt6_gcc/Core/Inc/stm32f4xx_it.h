/*
 * STM32F4 中断向量原型 - 仅 OpenLoad 用到的部分.
 *
 * F4 DMA controller 分 Stream + Channel (跟 F1 的 Channel 不同):
 *   USART1 RX: DMA2 Stream2 Channel4
 *   USART1 TX: DMA2 Stream7 Channel4
 *   USART2 RX: DMA1 Stream5 Channel4 (ESP8266)
 *   USART2 TX: DMA1 Stream6 Channel4 (ESP8266)
 */
#ifndef __STM32F4xx_IT_H
#define __STM32F4xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

void DMA2_Stream2_IRQHandler(void);   /* USART1 RX */
void DMA2_Stream7_IRQHandler(void);   /* USART1 TX */
void USART1_IRQHandler(void);

void DMA1_Stream5_IRQHandler(void);   /* USART2 RX (ESP8266) */
void DMA1_Stream6_IRQHandler(void);   /* USART2 TX (ESP8266) */
void USART2_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32F4xx_IT_H */
