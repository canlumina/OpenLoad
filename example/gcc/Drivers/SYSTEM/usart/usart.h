#ifndef __UART_H
#define __UART_H

#include "string.h"
#include "stdint.h"
#include "stdarg.h"
#include "stdio.h"
#include "stm32f1xx_hal_uart.h"
#include "stm32f1xx_hal_dma.h"

#define USART_UX USART1

#define U1_TX_SIZE 128
#define U1_RX_SIZE 128
#define U1_RX_MAX 10

#define U2_TX_SIZE 2048
#define U2_RX_SIZE 2048
#define U2_RX_MAX 256

#define U3_TX_SIZE 2048
#define U3_RX_SIZE 2048
#define U3_RX_MAX 256

typedef struct
{
	uint8_t *start;
	uint8_t *end;
} LCB;

typedef struct
{
	uint32_t RxCounter;
	uint32_t TxCounter;
	uint32_t TxState;
	LCB RxLocation[10];
	LCB TxLocation[10];
	LCB *RxInPtr;
	LCB *RxOutPtr;
	LCB *RxEndPtr;
	LCB *TxInPtr;
	LCB *TxOutPtr;
	LCB *TxEndPtr;
	UART_HandleTypeDef uart;
	DMA_HandleTypeDef dmatx;
	DMA_HandleTypeDef dmarx;

} UCB;

void u1_init(uint32_t bandrate);
void u2_init(uint32_t bandrate);
void u3_init(uint32_t bandrate);
void u1_ptrinit(void);
void u2_ptrinit(void);
void u3_ptrinit(void);
void u1_txdata(uint8_t *data, uint32_t data_len);
void u2_txdata(uint8_t *data, uint32_t data_len);
void u3_txdata(uint8_t *data, uint32_t data_len);
void u1_printf(char *fmt, ...);
void u2_printf(char *fmt, ...);
void u3_printf(char *fmt, ...);

extern UCB uart1;
extern UCB uart2;
extern UCB uart3;

#endif
