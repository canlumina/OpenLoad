/*
 * STM32F1 Port - UART2 裸驱动实现
 *
 * 结构与 port_io_uart.c (UART1) 同构: DMA1_Ch6 循环 RX + IDLE 中断 + ringbuf,
 * TX 阻塞. 但本模块不注册 ol_io_dev_t, 只给 esp8266 协议层调.
 */
#include "port_uart2.h"
#include "openload/ringbuf.h"
#include "openload/errno.h"
#include "stm32f1xx_hal.h"
#include "usart.h"
#include <stdint.h>

/* CubeMX 生成 (usart.c 内定义) */
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef  hdma_usart2_rx;

#define DMA_RX_SIZE     256
#define RX_FIFO_SIZE    2048       /* ESP8266 +IPD 可能突发数百字节, 留足余量 */

static uint8_t      s_dma_rx[DMA_RX_SIZE];
static uint8_t      s_rx_fifo_storage[RX_FIFO_SIZE];
static ol_ringbuf_t s_rx_fifo;
static volatile uint16_t s_last_dma_pos;
static uint8_t      s_inited;

/* 把 [last_pos .. cur_pos) 的 DMA 数据搬入 ringbuf. 与 UART1 同结构,
 * 用 PRIMASK 关全局中断防 DMA ISR / IDLE ISR / 主线程读三处重入. */
static void drain_dma_to_fifo(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    uint16_t cur = DMA_RX_SIZE - (uint16_t)__HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
    if (cur != s_last_dma_pos) {
        if (cur > s_last_dma_pos) {
            ol_ringbuf_write(&s_rx_fifo, &s_dma_rx[s_last_dma_pos],
                             cur - s_last_dma_pos);
        } else {
            ol_ringbuf_write(&s_rx_fifo, &s_dma_rx[s_last_dma_pos],
                             DMA_RX_SIZE - s_last_dma_pos);
            if (cur > 0) {
                ol_ringbuf_write(&s_rx_fifo, &s_dma_rx[0], cur);
            }
        }
        s_last_dma_pos = cur;
    }

    __set_PRIMASK(primask);
}

/* -------- IRQ Hooks -------- */
void port_uart2_idle_isr(void)  { drain_dma_to_fifo(); }
void port_uart2_dma_half(void)  { drain_dma_to_fifo(); }
void port_uart2_dma_full(void)  { drain_dma_to_fifo(); }

/* -------- 业务接口 -------- */
int port_uart2_init(uint32_t baud)
{
    if (s_inited) { return OL_OK; }

    if (ol_ringbuf_init(&s_rx_fifo, s_rx_fifo_storage, RX_FIFO_SIZE) != OL_OK) {
        return OL_E_INVAL;
    }
    s_last_dma_pos = 0;

    /* usart.c 里 MX_USART2_UART_Init 已经按 OPENLOAD_ESP_UART_BAUD 启好.
       若 baud 跟当前不一致 (例如运行时 setbaud 切换), 走 deinit + 重 init. */
    if (baud && baud != huart2.Init.BaudRate) {
        HAL_UART_DeInit(&huart2);
        huart2.Init.BaudRate = baud;
        if (HAL_UART_Init(&huart2) != HAL_OK) { return OL_E_IO; }
    }

    if (HAL_UART_Receive_DMA(&huart2, s_dma_rx, DMA_RX_SIZE) != HAL_OK) {
        return OL_E_IO;
    }
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);

    s_inited = 1;
    return OL_OK;
}

int port_uart2_setbaud(uint32_t baud)
{
    if (!s_inited) { return port_uart2_init(baud); }
    if (baud == huart2.Init.BaudRate) { return OL_OK; }
    HAL_UART_DMAStop(&huart2);
    HAL_UART_DeInit(&huart2);
    huart2.Init.BaudRate = baud;
    if (HAL_UART_Init(&huart2) != HAL_OK)            { return OL_E_IO; }
    s_last_dma_pos = 0;
    ol_ringbuf_reset(&s_rx_fifo);
    if (HAL_UART_Receive_DMA(&huart2, s_dma_rx, DMA_RX_SIZE) != HAL_OK) {
        return OL_E_IO;
    }
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
    return OL_OK;
}

int port_uart2_write(const uint8_t *buf, uint32_t len)
{
    HAL_StatusTypeDef st = HAL_UART_Transmit(&huart2, (uint8_t *)buf,
                                             (uint16_t)len, HAL_MAX_DELAY);
    return (st == HAL_OK) ? (int)len : OL_E_IO;
}

int port_uart2_read(uint8_t *buf, uint32_t len)
{
    drain_dma_to_fifo();
    return (int)ol_ringbuf_read(&s_rx_fifo, buf, len);
}

int port_uart2_available(void)
{
    drain_dma_to_fifo();
    return (int)ol_ringbuf_used(&s_rx_fifo);
}

void port_uart2_rx_flush(void)
{
    drain_dma_to_fifo();
    ol_ringbuf_reset(&s_rx_fifo);
}
