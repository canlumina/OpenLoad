/*
 * STM32F4 Port - UART2 裸驱动 (供 ESP8266 / 其它 AT 模块使用)
 *
 * 引脚:  PA2 = TX,  PA3 = RX (AF7_USART2)
 * 时钟:  APB1 42 MHz
 * DMA:   RX = DMA1_Stream5 Channel4 循环, TX = DMA1_Stream6 Channel4 普通
 *        (M1 阶段 TX 仍走阻塞, 不挂 DMA TX)
 * IRQ:   USART2_IRQn(5), DMA1_Stream5_IRQn(4), DMA1_Stream6_IRQn(4)
 *
 * 与 UART1 同构: DMA 循环 + IDLE 中断 + ringbuf, RX 字节流接口与 console 一致.
 * 但 UART2 不实现 ol_io_dev_t (不参与 console/log), 仅给 esp8266 模块用.
 *
 * 编译开关: OPENLOAD_ENABLE_ESP8266 启用时本模块编入, 否则整文件不参与构建.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int  port_uart2_init(uint32_t baud);
int  port_uart2_setbaud(uint32_t baud);
int  port_uart2_write(const uint8_t *buf, uint32_t len);
int  port_uart2_read(uint8_t *buf, uint32_t len);
int  port_uart2_available(void);
void port_uart2_rx_flush(void);             /* 清空 ringbuf */

/* IRQ hooks — 由 port_io_uart.c 的 HAL_UART_Rx*CpltCallback dispatcher
 * 与 stm32f4xx_it.c 的 USART2_IRQHandler 分发调用 */
void port_uart2_idle_isr(void);
void port_uart2_dma_half(void);
void port_uart2_dma_full(void);

#ifdef __cplusplus
}
#endif
