#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "usart.h"
#include "ringbuff.h"
#include "dev_usart.h"

/*当前文件需要根据平台设备定义UART设备使用DMA和ringbuff*/

/* UART设备结构体定义 */
typedef struct
{
	uint8_t tx_busy;		    /*!< 发送忙标志 */
	_fifo_t tx_fifo;	        /*!< 发送FIFO缓冲区 */
	_fifo_t rx_fifo;	        /*!< 接收FIFO缓冲区 */
	uint8_t *dmarx_buf;	        /*!< DMA接收缓冲区指针 */
	uint16_t dmarx_buf_size;    /*!< DMA接收缓冲区大小 */
	uint8_t *dmatx_buf;	        /*!< DMA发送缓冲区指针 */
	uint16_t dmatx_buf_size;    /*!< DMA发送缓冲区大小 */
	uint16_t last_dmarx_size;   /*!< 上次DMA接收处理的位置 */
	
	//  ！！！这里替换成平台设备的UART句柄
	UART_HandleTypeDef *huart;  /*!< UART句柄指针 */   
	
}uart_device_t;

#define UART1_TX_BUF_SIZE           2048
#define UART1_RX_BUF_SIZE           2048
#define	UART1_DMA_RX_BUF_SIZE		1024
#define	UART1_DMA_TX_BUF_SIZE		512

#define UART2_TX_BUF_SIZE           2048
#define UART2_RX_BUF_SIZE           2048
#define	UART2_DMA_RX_BUF_SIZE		1024
#define	UART2_DMA_TX_BUF_SIZE		512


static uint8_t s_uart1_tx_buf[UART1_TX_BUF_SIZE];
static uint8_t s_uart1_rx_buf[UART1_RX_BUF_SIZE];
static uint8_t s_uart1_dmarx_buf[UART1_DMA_RX_BUF_SIZE];
static uint8_t s_uart1_dmatx_buf[UART1_DMA_TX_BUF_SIZE];

static uint8_t s_uart2_tx_buf[UART2_TX_BUF_SIZE];
static uint8_t s_uart2_rx_buf[UART2_RX_BUF_SIZE];
static uint8_t s_uart2_dmarx_buf[UART2_DMA_RX_BUF_SIZE];
static uint8_t s_uart2_dmatx_buf[UART2_DMA_TX_BUF_SIZE];

static uart_device_t s_uart_dev[2] = {0};

uint32_t s_UartTxRxCount[4] = {0};


/* FIFO加锁函数 */
static void fifo_lock(void)
{
    __disable_irq();
}

/* FIFO解锁函数 */
static void fifo_unlock(void)
{
    __enable_irq();
}


/**
 * @brief UART DMA接收半满中断处理
 * @param uart_id UART设备ID
 * @retval 无
 */
void uart_dmarx_half_done_isr(uint8_t uart_id)
{
	uint16_t recv_total_size;
	uint16_t recv_size;
	
	/* 获取已接收的总数据量 = 缓冲区大小 - DMA剩余计数 */
	recv_total_size = s_uart_dev[uart_id].dmarx_buf_size - __HAL_DMA_GET_COUNTER(s_uart_dev[uart_id].huart->hdmarx);
	recv_size = recv_total_size - s_uart_dev[uart_id].last_dmarx_size;
	
	if (recv_size > 0)
	{
		s_UartTxRxCount[uart_id*2+1] += recv_size;
		fifo_write(&s_uart_dev[uart_id].rx_fifo, 
				   &s_uart_dev[uart_id].dmarx_buf[s_uart_dev[uart_id].last_dmarx_size], 
				   recv_size);
		s_uart_dev[uart_id].last_dmarx_size = recv_total_size;
	}
}

/**
 * @brief UART DMA接收完成中断处理
 * @param uart_id UART设备ID
 * @retval 无
 */
void uart_dmarx_done_isr(uint8_t uart_id)
{
	uint16_t recv_size;
	
	/* DMA完成，处理剩余数据 */
	recv_size = s_uart_dev[uart_id].dmarx_buf_size - s_uart_dev[uart_id].last_dmarx_size;
	
	if (recv_size > 0)
	{
		s_UartTxRxCount[uart_id*2+1] += recv_size;
		fifo_write(&s_uart_dev[uart_id].rx_fifo, 
				   &s_uart_dev[uart_id].dmarx_buf[s_uart_dev[uart_id].last_dmarx_size], 
				   recv_size);
	}
	
	/* 循环模式下，DMA会自动从0开始 */
	s_uart_dev[uart_id].last_dmarx_size = 0;
}

/**
 * @brief UART空闲中断处理
 * @param uart_id UART设备ID
 * @retval 无
 */
void uart_dmarx_idle_isr(uint8_t uart_id)
{
	uint16_t recv_total_size;
	uint16_t recv_size;
	
	/* 获取已接收的总数据量 = 缓冲区大小 - DMA剩余计数 */
	recv_total_size = s_uart_dev[uart_id].dmarx_buf_size - __HAL_DMA_GET_COUNTER(s_uart_dev[uart_id].huart->hdmarx);
	recv_size = recv_total_size - s_uart_dev[uart_id].last_dmarx_size;
	
	if (recv_size > 0)
	{
		s_UartTxRxCount[uart_id*2+1] += recv_size;
		fifo_write(&s_uart_dev[uart_id].rx_fifo, 
				   &s_uart_dev[uart_id].dmarx_buf[s_uart_dev[uart_id].last_dmarx_size], 
				   recv_size);
		s_uart_dev[uart_id].last_dmarx_size = recv_total_size;
	}
}

/**
 * @brief HAL库UART接收半满回调函数
 * @param huart UART句柄
 * @retval 无
 */
void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
	uint8_t uart_id = 0;
	
	if (huart->Instance == USART1)
	{
		uart_id = 0;
	}
	else if (huart->Instance == USART2)
	{
		uart_id = 1;
	}
	
	uart_dmarx_half_done_isr(uart_id);
}

/**
 * @brief HAL库UART接收完成回调函数
 * @param huart UART句柄
 * @retval 无
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	uint8_t uart_id = 0;
	
	if (huart->Instance == USART1)
	{
		uart_id = 0;
	}
	else if (huart->Instance == USART2)
	{
		uart_id = 1;
	}
	
	uart_dmarx_done_isr(uart_id);
}

/**
 * @brief HAL库UART发送完成回调函数
 * @param huart UART句柄
 * @retval 无
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	uint8_t uart_id = 0;
	
	if (huart->Instance == USART1)
	{
		uart_id = 0;
	}
	else if (huart->Instance == USART2)
	{
		uart_id = 1;
	}
	
	/* 清除发送忙标志 */
	s_uart_dev[uart_id].tx_busy = 0;
	
	/* 继续发送FIFO中的数据 */
	uart_poll_dma_tx(uart_id);
}





/**
 * @brief 初始化UART设备
 * @param uart_id UART设备ID
 * @retval 无
 */
void uart_device_init(uint8_t uart_id)
{
  	if (uart_id == 0)
	{
		/* 初始化UART1的发送和接收FIFO */
		fifo_register(&s_uart_dev[uart_id].tx_fifo, &s_uart1_tx_buf[0], 
                      sizeof(s_uart1_tx_buf), fifo_lock, fifo_unlock);
		fifo_register(&s_uart_dev[uart_id].rx_fifo, &s_uart1_rx_buf[0], 
                      sizeof(s_uart1_rx_buf), fifo_lock, fifo_unlock);
		
		/* 配置UART1的DMA发送和接收缓冲区 */
		s_uart_dev[uart_id].dmarx_buf = &s_uart1_dmarx_buf[0];
		s_uart_dev[uart_id].dmarx_buf_size = sizeof(s_uart1_dmarx_buf);
		s_uart_dev[uart_id].dmatx_buf = &s_uart1_dmatx_buf[0];
		s_uart_dev[uart_id].dmatx_buf_size = sizeof(s_uart1_dmatx_buf);
		s_uart_dev[uart_id].huart = &huart1;
		s_uart_dev[uart_id].tx_busy = 0;
		s_uart_dev[uart_id].last_dmarx_size = 0;
		
		/* 直接启动DMA循环接收 */
		HAL_UART_Receive_DMA(&huart1, 
							 s_uart_dev[uart_id].dmarx_buf, 
							 s_uart_dev[uart_id].dmarx_buf_size);
		/* 使能空闲中断 */
		__HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
	}
	else if (uart_id == 1)
	{
		/* 初始化UART2的发送和接收FIFO */
		fifo_register(&s_uart_dev[uart_id].tx_fifo, &s_uart2_tx_buf[0], 
                      sizeof(s_uart2_tx_buf), fifo_lock, fifo_unlock);
		fifo_register(&s_uart_dev[uart_id].rx_fifo, &s_uart2_rx_buf[0], 
                      sizeof(s_uart2_rx_buf), fifo_lock, fifo_unlock);
		
		/* 配置UART2的DMA发送和接收缓冲区 */
		s_uart_dev[uart_id].dmarx_buf = &s_uart2_dmarx_buf[0];
		s_uart_dev[uart_id].dmarx_buf_size = sizeof(s_uart2_dmarx_buf);
		s_uart_dev[uart_id].dmatx_buf = &s_uart2_dmatx_buf[0];
		s_uart_dev[uart_id].dmatx_buf_size = sizeof(s_uart2_dmatx_buf);
		s_uart_dev[uart_id].huart = &huart2;
		s_uart_dev[uart_id].tx_busy = 0;
		s_uart_dev[uart_id].last_dmarx_size = 0;
		
		/* 直接启动DMA循环接收 */
		HAL_UART_Receive_DMA(&huart2, 
							 s_uart_dev[uart_id].dmarx_buf, 
							 s_uart_dev[uart_id].dmarx_buf_size);
		/* 使能空闲中断 */
		__HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
	}
}

/**
 * @brief  向指定UART设备写入数据，数据将被写入发送FIFO缓冲区
 * @param  uart_id UART设备ID
 * @param  buf     待发送数据缓冲区指针
 * @param  size    待发送数据长度
 * @retval 实际写入FIFO的数据长度
 */
uint16_t uart_write(uint8_t uart_id, const uint8_t *buf, uint16_t size)
{
	return fifo_write(&s_uart_dev[uart_id].tx_fifo, buf, size);
}

/**
 * @brief  从指定UART设备读取数据，数据来自接收FIFO缓冲区
 * @param  uart_id UART设备ID
 * @param  buf     接收数据缓冲区指针
 * @param  size    期望读取的数据长度
 * @retval 实际从FIFO读取的数据长度
 */
uint16_t uart_read(uint8_t uart_id, uint8_t *buf, uint16_t size)
{
	return fifo_read(&s_uart_dev[uart_id].rx_fifo, buf, size);
}


/**
 * @brief  轮询UART发送FIFO，如有数据则通过DMA发送
 * @param  uart_id UART设备ID
 * @retval 无
 */
void uart_poll_dma_tx(uint8_t uart_id)
{
  	uint16_t size = 0;
	
	/* 如果DMA正在发送，直接返回 */
	if (s_uart_dev[uart_id].tx_busy)
    {
        return;
    }
	
	/* 从发送FIFO读取数据 */
	size = fifo_read(&s_uart_dev[uart_id].tx_fifo, 
					 s_uart_dev[uart_id].dmatx_buf,
					 s_uart_dev[uart_id].dmatx_buf_size);
	
	if (size != 0)
	{	
        s_UartTxRxCount[uart_id*2+0] += size;
		s_uart_dev[uart_id].tx_busy = 1;	/* 设置发送忙标志 */
		
		/* 使用HAL_UART_Transmit_DMA发送数据 */
		HAL_UART_Transmit_DMA(s_uart_dev[uart_id].huart, 
							  s_uart_dev[uart_id].dmatx_buf, 
							  size);
	}
}


void uart1_printf(const char* str)
{
    uart_write(DEV_UART1, (uint8_t*)str, strlen(str));
    uart_poll_dma_tx(DEV_UART1);
}

void uart2_printf(const char* str)
{
    uart_write(DEV_UART1, (uint8_t*)str, strlen(str));
    uart_poll_dma_tx(DEV_UART1);
}
