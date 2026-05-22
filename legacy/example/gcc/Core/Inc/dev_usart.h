#ifndef _DEV_USART_H_
#define _DEV_USART_H_

#include <stdbool.h>
#include <stdint.h>

#define DEV_UART1	0
#define DEV_UART2	1

extern void uart_device_init(uint8_t uart_id);
extern uint16_t uart_write(uint8_t uart_id, const uint8_t *buf, uint16_t size);
extern uint16_t uart_read(uint8_t uart_id, uint8_t *buf, uint16_t size);
extern void uart_poll_dma_tx(uint8_t uart_id);

extern uint32_t s_UartTxRxCount[4];

#endif 