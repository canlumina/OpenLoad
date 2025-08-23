#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "usart.h"
#include "ringbuff.h"
#include "dev_usart.h"

/* UART设备结构体定义 */
typedef struct
{
	uint8_t status;		        /*!< 发送状态标志 */
	_fifo_t tx_fifo;	        /*!< 发送FIFO缓冲区 */
	_fifo_t rx_fifo;	        /*!< 接收FIFO缓冲区 */
	uint8_t *dmarx_buf;	        /*!< DMA接收缓冲区指针 */
	uint16_t dmarx_buf_size;    /*!< DMA接收缓冲区大小 */
	uint8_t *dmatx_buf;	        /*!< DMA发送缓冲区指针 */
	uint16_t dmatx_buf_size;    /*!< DMA发送缓冲区大小 */
	uint16_t last_dmarx_size;   /*!< DMA上次接收数据大小 */
}uart_device_t;

#define UART1_TX_BUF_SIZE           512
#define UART1_RX_BUF_SIZE           1024
#define	UART1_DMA_RX_BUF_SIZE		1024
#define	UART1_DMA_TX_BUF_SIZE		128


static uint8_t s_uart1_tx_buf[UART1_TX_BUF_SIZE];
static uint8_t s_uart1_rx_buf[UART1_RX_BUF_SIZE];
static uint8_t s_uart1_dmarx_buf[UART1_DMA_RX_BUF_SIZE] __attribute__((at(0X20001000)));
static uint8_t s_uart1_dmatx_buf[UART1_DMA_TX_BUF_SIZE];

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
 * @brief 配置UART1的DMA接收参数并启动DMA接收
 * @param mem_addr DMA接收缓冲区地址
 * @param mem_size DMA接收缓冲区大小
 * @retval 无
 */
void bsp_uart1_dmarx_config(uint8_t *mem_addr, uint32_t mem_size)
{
	__HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
	HAL_UART_Receive_DMA(&huart1, mem_addr, mem_size);
}

/**
 * @brief 配置UART1的DMA发送参数并启动DMA发送
 * @param mem_addr DMA发送缓冲区地址
 * @param mem_size DMA发送数据大小
 * @retval 无
 */
void bsp_uart1_dmatx_config(uint8_t *mem_addr, uint32_t mem_size)
{
	HAL_UART_Transmit_DMA(&huart1, mem_addr, mem_size);
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
		bsp_uart1_dmarx_config(s_uart_dev[uart_id].dmarx_buf, 
							   sizeof(s_uart1_dmarx_buf));/* 启动UART1的DMA接收功能 */
		s_uart_dev[uart_id].status  = 0;
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

extern DMA_HandleTypeDef hdma_usart1_rx;
uint16_t bsp_uart1_get_dmarx_buf_remain_size(void)
{
  return __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);
}

/**
 * @brief  UART DMA接收完成中断处理函数
 * @param  uart_id UART设备ID
 * @retval 无
 */
void uart_dmarx_done_isr(uint8_t uart_id)
{
  	uint16_t recv_size;
	
	recv_size = s_uart_dev[uart_id].dmarx_buf_size - s_uart_dev[uart_id].last_dmarx_size;
    s_UartTxRxCount[uart_id*2+1] += recv_size;
	fifo_write(&s_uart_dev[uart_id].rx_fifo, 
				   (const uint8_t *)&(s_uart_dev[uart_id].dmarx_buf[s_uart_dev[uart_id].last_dmarx_size]), recv_size);

	s_uart_dev[uart_id].last_dmarx_size = 0;
}

/**
 * @brief  UART DMA接收半完成中断处理函数
 * @param  uart_id UART设备ID
 * @retval 无
 */
void uart_dmarx_half_done_isr(uint8_t uart_id)
{
  	uint16_t recv_total_size;
  	uint16_t recv_size;
	
	if(uart_id == 0)
	{
	  	recv_total_size = s_uart_dev[uart_id].dmarx_buf_size - bsp_uart1_get_dmarx_buf_remain_size();
	}

	recv_size = recv_total_size - s_uart_dev[uart_id].last_dmarx_size;
	s_UartTxRxCount[uart_id*2+1] += recv_size;
    
	fifo_write(&s_uart_dev[uart_id].rx_fifo, 
				   (const uint8_t *)&(s_uart_dev[uart_id].dmarx_buf[s_uart_dev[uart_id].last_dmarx_size]), recv_size);
	s_uart_dev[uart_id].last_dmarx_size = recv_total_size;
}

/**
 * @brief  UART DMA接收缓冲区空闲中断处理函数
 * @param  uart_id UART设备ID
 * @retval 无
 */
void uart_dmarx_idle_isr(uint8_t uart_id)
{
  	uint16_t recv_total_size;
  	uint16_t recv_size;
	
	if(uart_id == 0)
	{
	  	recv_total_size = s_uart_dev[uart_id].dmarx_buf_size - bsp_uart1_get_dmarx_buf_remain_size();
	}

	recv_size = recv_total_size - s_uart_dev[uart_id].last_dmarx_size;
	s_UartTxRxCount[uart_id*2+1] += recv_size;
	fifo_write(&s_uart_dev[uart_id].rx_fifo, 
				   (const uint8_t *)&(s_uart_dev[uart_id].dmarx_buf[s_uart_dev[uart_id].last_dmarx_size]), recv_size);
	s_uart_dev[uart_id].last_dmarx_size = recv_total_size;
}
/**
 * @brief  UART DMA发送完成中断处理函数
 * @param  uart_id UART设备ID
 * @retval 无
 */
void uart_dmatx_done_isr(uint8_t uart_id)
{
 	s_uart_dev[uart_id].status = 0;	/* 清除DMA发送标志 */
}

/**
 * @brief  轮询UART发送FIFO，如有数据则通过DMA发送
 * @param  uart_id UART设备ID
 * @retval 无
 */
void uart_poll_dma_tx(uint8_t uart_id)
{
  	uint16_t size = 0;
	
	if (0x01 == s_uart_dev[uart_id].status)
    {
        return;
    }
	size = fifo_read(&s_uart_dev[uart_id].tx_fifo, s_uart_dev[uart_id].dmatx_buf,
					 s_uart_dev[uart_id].dmatx_buf_size);
	if (size != 0)
	{	
        s_UartTxRxCount[uart_id*2+0] += size;
	  	if (uart_id == 0)
		{
            s_uart_dev[uart_id].status = 0x01;	/* 设置DMA发送状态 */
		  	bsp_uart1_dmatx_config(s_uart_dev[uart_id].dmatx_buf, size);
		}

	}
}