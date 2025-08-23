#ifndef _RINGBUFF_H_
#define _RINGBUFF_H_
 
#include <stdbool.h>
#include <stdint.h>

typedef void (*lock_fun)(void);

typedef struct
{
	uint8_t *buf;      	    /* FIFO缓冲区指针 */
	uint32_t buf_size;    	/* FIFO缓冲区总大小 */
    uint32_t occupy_size;   /* 已使用的缓冲区大小 */
	uint8_t *pwrite;      	/* 写指针 */
	uint8_t *pread;       	/* 读指针 */
	void (*lock)(void);	/* 加锁函数指针 */
	void (*unlock)(void);	/* 解锁函数指针 */
}_fifo_t;
 
 
extern void fifo_register(_fifo_t *pfifo, uint8_t *pfifo_buf, uint32_t size,
                            lock_fun lock, lock_fun unlock);
extern void fifo_release(_fifo_t *pfifo);
extern uint32_t fifo_write(_fifo_t *pfifo, const uint8_t *pbuf, uint32_t size);
extern uint32_t fifo_read(_fifo_t *pfifo, uint8_t *pbuf, uint32_t size);
extern uint32_t fifo_get_total_size(_fifo_t *pfifo);
extern uint32_t fifo_get_free_size(_fifo_t *pfifo);
extern uint32_t fifo_get_occupy_size(_fifo_t *pfifo);

#endif