
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "ringbuff.h"
#include <stddef.h>

/**
  * @brief  初始化FIFO缓冲区
  * @param  pfifo: FIFO结构体指针
		    pfifo_buf: FIFO缓冲区指针
		    size: 缓冲区大小
  * @retval none
*/
void fifo_register(_fifo_t *pfifo, uint8_t *pfifo_buf, uint32_t size,
                   lock_fun lock, lock_fun unlock)
{
	pfifo->buf_size = size;
	pfifo->buf 	= pfifo_buf;
	pfifo->pwrite = pfifo->buf;
	pfifo->pread  = pfifo->buf;
    pfifo->occupy_size = 0;
    pfifo->lock = lock;
    pfifo->unlock = unlock;
}
 
/**
  * @brief  释放FIFO缓冲区
  * @param  pfifo: FIFO结构体指针
  * @retval none
*/
void fifo_release(_fifo_t *pfifo)
{
	pfifo->buf_size = 0;
    pfifo->occupy_size = 0;
	pfifo->buf 	= NULL;
	pfifo->pwrite = 0;
	pfifo->pread  = 0;
    pfifo->lock = NULL;
    pfifo->unlock = NULL; 
}
 
/**
  * @brief  向FIFO写入数据
  * @param  pfifo: FIFO结构体指针
		    pbuf: 写入数据缓冲区
		    size: 写入数据长度
  * @retval 实际写入长度
*/
uint32_t fifo_write(_fifo_t *pfifo, const uint8_t *pbuf, uint32_t size)
{
	uint32_t w_size= 0,free_size = 0;
	
	if ((size==0) || (pfifo==NULL) || (pbuf==NULL))
	{
		return 0;
	}
 
    free_size = fifo_get_free_size(pfifo);
    if(free_size == 0)
    {
        return 0;
    }

    if(free_size < size)
    {
        size = free_size;
    }
	w_size = size;
    if (pfifo->lock != NULL)
        pfifo->lock();
	while(w_size-- > 0)
	{
		*pfifo->pwrite++ = *pbuf++;
		if (pfifo->pwrite >= (pfifo->buf + pfifo->buf_size)) 
		{
			pfifo->pwrite = pfifo->buf;
		}
        pfifo->occupy_size++;
	}
    if (pfifo->unlock != NULL)
        pfifo->unlock();
	return size;
}
 
/**
  * @brief  从FIFO读取数据
  * @param  pfifo: FIFO结构体指针
		    pbuf: 读取数据存放缓冲区
		    size: 读取数据长度
  * @retval 实际读取长度
*/
uint32_t fifo_read(_fifo_t *pfifo, uint8_t *pbuf, uint32_t size)
{
	uint32_t r_size = 0,occupy_size = 0;
	
	if ((size==0) || (pfifo==NULL) || (pbuf==NULL))
	{
		return 0;
	}
    
    occupy_size = fifo_get_occupy_size(pfifo);
    if(occupy_size == 0)
    {
        return 0;
    }

    if(occupy_size < size)
    {
        size = occupy_size;
    }
    if (pfifo->lock != NULL)
        pfifo->lock();
	r_size = size;
	while(r_size-- > 0)
	{
		*pbuf++ = *pfifo->pread++;
		if (pfifo->pread >= (pfifo->buf + pfifo->buf_size)) 
		{
			pfifo->pread = pfifo->buf;
		}
        pfifo->occupy_size--;
	}
    if (pfifo->unlock != NULL)
        pfifo->unlock();
	return size;
}
 

/**
  * @brief  获取FIFO缓冲区总大小
  * @param  pfifo: FIFO结构体指针
  * @retval 缓冲区总大小
*/
uint32_t fifo_get_total_size(_fifo_t *pfifo)
{
	if (pfifo==NULL)
		return 0;
	
	return pfifo->buf_size;
}
 

/**
  * @brief  获取FIFO缓冲区空闲大小
  * @param  pfifo: FIFO结构体指针
  * @retval 空闲缓冲区大小
*/
uint32_t fifo_get_free_size(_fifo_t *pfifo)
{
	uint32_t size;
 
	if (pfifo==NULL)
		return 0;
	
    size = pfifo->buf_size - fifo_get_occupy_size(pfifo);

	return size;
}
 

/**
  * @brief  获取FIFO缓冲区已使用大小
  * @param  pfifo: FIFO结构体指针
  * @retval 已使用缓冲区大小
*/
uint32_t fifo_get_occupy_size(_fifo_t *pfifo)
{
	if (pfifo==NULL)
		return 0;
    
	return  pfifo->occupy_size;
}