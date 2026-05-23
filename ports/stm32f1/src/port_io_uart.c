/*
 * STM32F1 Port - UART1 console (ol_io_ops_t)
 *
 * RX: DMA1_Ch5 循环模式 + 空闲中断, 中断里把新增数据从 DMA buffer 搬入 ringbuf
 * TX: HAL_UART_Transmit 阻塞模式 (省 TX FIFO/DMA TX 中断, 体积更小)
 *
 * 中断流程:
 *   stm32f1xx_it.c::USART1_IRQHandler()
 *     ├── 若 IDLE 置位: port_uart1_idle_isr() ← 本文件实现
 *     │      └── 把 DMA buffer 新增数据 → ringbuf
 *     └── HAL_UART_IRQHandler(&huart1)
 *            ├── 触发 HAL_UART_RxHalfCpltCallback() ← 本文件实现
 *            └── 触发 HAL_UART_RxCpltCallback()     ← 本文件实现
 */
#include "openload/ops/io_ops.h"
#include "openload/ringbuf.h"
#include "openload/errno.h"
#include "port_stm32f1.h"
#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <string.h>

/* CubeMX 生成的全局句柄 — 通常在 usart.c/dma.c 内定义 */
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef  hdma_usart1_rx;

#define DMA_RX_SIZE     256                 /* 循环模式 DMA buffer */
#define RX_FIFO_SIZE    2048                /* 必须是 2 的幂; ≥ 1K XMODEM 帧 1029B */

static uint8_t      s_dma_rx[DMA_RX_SIZE];
static uint8_t      s_rx_fifo_storage[RX_FIFO_SIZE];
static ol_ringbuf_t s_rx_fifo;
static volatile uint16_t s_last_dma_pos;

/* 把 [last_pos .. cur_pos) 的 DMA 数据搬入 ringbuf. 循环模式下 cur_pos 可能回卷.
 *
 * 重入保护: 本函数被 USART1 IDLE ISR (优先级 5) + DMA1_Ch5 HT/TC ISR (优先级 4)
 * + 主线程 uart_read 三处调用. DMA ISR 优先级数字更小 -> 能 preempt USART ISR,
 * 重入会破坏 s_last_dma_pos / ringbuf head. 用 PRIMASK 关全局中断包裹整个搬运,
 * 几十字节 memcpy 微秒级, 对系统延迟无影响. */
static void drain_dma_to_fifo(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    uint16_t cur = DMA_RX_SIZE - (uint16_t)__HAL_DMA_GET_COUNTER(&hdma_usart1_rx);
    if (cur != s_last_dma_pos) {
        if (cur > s_last_dma_pos) {
            ol_ringbuf_write(&s_rx_fifo, &s_dma_rx[s_last_dma_pos], cur - s_last_dma_pos);
        } else {
            /* 回卷: 先写 [last_pos..end), 再写 [0..cur) */
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

/* -------- IRQ Hooks (USART1_IRQHandler 内部调用) -------- */

void port_uart1_idle_isr(void)
{
    /* 由 it.c 在清 IDLE 标志之后调用 */
    drain_dma_to_fifo();
}

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        drain_dma_to_fifo();
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        drain_dma_to_fifo();
        /* 循环模式下 DMA 自动回卷, last_pos 已在 drain 中追平 */
    }
}

/* -------- ol_io_ops_t -------- */

static int uart_read(ol_io_dev_t *dev, uint8_t *buf, uint32_t len)
{
    (void)dev;
    /* 在 read 路径上也 drain 一次, 兜底空闲中断在中断关闭时被吞掉的情况 */
    drain_dma_to_fifo();
    return (int)ol_ringbuf_read(&s_rx_fifo, buf, len);
}

static int uart_write(ol_io_dev_t *dev, const uint8_t *buf, uint32_t len)
{
    (void)dev;
    /* 阻塞 TX, 简化实现 + 节省 ROM. bootloader 不在意吞吐. */
    HAL_StatusTypeDef st = HAL_UART_Transmit(&huart1, (uint8_t *)buf,
                                             (uint16_t)len, HAL_MAX_DELAY);
    return (st == HAL_OK) ? (int)len : OL_E_IO;
}

static int uart_available(ol_io_dev_t *dev)
{
    (void)dev;
    drain_dma_to_fifo();
    return (int)ol_ringbuf_used(&s_rx_fifo);
}

static int uart_flush(ol_io_dev_t *dev)
{
    (void)dev;
    /* 阻塞 TX 已经在 write 返回时 flush 完毕 */
    return OL_OK;
}

static const ol_io_ops_t s_uart_ops = {
    .read      = uart_read,
    .write     = uart_write,
    .available = uart_available,
    .flush     = uart_flush,
};

static ol_io_dev_t s_console_dev = {
    .name = "console",
    .ops  = &s_uart_ops,
    .priv = 0,
};

OL_IO_DEV_REGISTER(console, &s_console_dev);

/* 由 board_init 在 MX_USART1_UART_Init / DMA init 之后调用 */
int port_uart1_init(void)
{
    if (ol_ringbuf_init(&s_rx_fifo, s_rx_fifo_storage, RX_FIFO_SIZE) != OL_OK) {
        return OL_E_INVAL;
    }
    s_last_dma_pos = 0;
    if (HAL_UART_Receive_DMA(&huart1, s_dma_rx, DMA_RX_SIZE) != HAL_OK) {
        return OL_E_IO;
    }
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
    return OL_OK;
}
