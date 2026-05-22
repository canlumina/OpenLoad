#ifndef _DEV_USART_H_
#define _DEV_USART_H_

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>



extern void uart_device_init(uint8_t uart_id);
extern uint16_t uart_write(uint8_t uart_id, const uint8_t *buf, uint16_t size);
extern uint16_t uart_read(uint8_t uart_id, uint8_t *buf, uint16_t size);
extern void uart_poll_dma_tx(uint8_t uart_id);

extern uint32_t s_UartTxRxCount[4];

int uart_printf(const char* format, ...);
void uart1_printf(const char* str);
void uart2_printf(const char* str);


#define FORMAT_PRINTF	uart_printf
// 需要使用格式化输出的时候才建议使用该函数，毕竟开销大
#define log_d(...)  FORMAT_PRINTF("(%s:%" PRIdLEAST16 ") ", __func__, __LINE__); FORMAT_PRINTF(__VA_ARGS__); FORMAT_PRINTF("\r\n")
#define log_f(...)  FORMAT_PRINTF("\033[32;22m"); FORMAT_PRINTF(__VA_ARGS__); FORMAT_PRINTF("\r\n");FORMAT_PRINTF("\033[0m")

// 不需要使用格式化输出的时候建议使用该函数，开销小
#define log_i(src)		uart1_printf(src); uart1_printf("\r\n")

#endif 
