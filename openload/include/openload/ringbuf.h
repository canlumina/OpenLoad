/*
 * OpenLoad - 通用环形缓冲区 (核心内部工具)
 *
 * 单生产者-单消费者无锁, 用 head/tail 索引。容量必须是 2 的幂以便用与运算
 * 取模 (实现里强制要求, 由 ol_ringbuf_init 校验)。
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t  *buf;
    uint32_t  capacity;     /* 必须是 2 的幂 */
    uint32_t  mask;         /* capacity - 1 */
    volatile uint32_t head; /* 写指针 */
    volatile uint32_t tail; /* 读指针 */
} ol_ringbuf_t;

/** capacity 必须是 2 的幂, 否则返回 OL_E_INVAL. */
int      ol_ringbuf_init(ol_ringbuf_t *rb, uint8_t *storage, uint32_t capacity);

uint32_t ol_ringbuf_write(ol_ringbuf_t *rb, const uint8_t *data, uint32_t len);
uint32_t ol_ringbuf_read(ol_ringbuf_t *rb, uint8_t *data, uint32_t len);

uint32_t ol_ringbuf_used(const ol_ringbuf_t *rb);
uint32_t ol_ringbuf_free(const ol_ringbuf_t *rb);
bool     ol_ringbuf_empty(const ol_ringbuf_t *rb);
bool     ol_ringbuf_full(const ol_ringbuf_t *rb);
void     ol_ringbuf_reset(ol_ringbuf_t *rb);
