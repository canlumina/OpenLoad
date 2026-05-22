/*
 * OpenLoad - 环形缓冲实现
 */
#include "openload/ringbuf.h"
#include "openload/errno.h"
#include <string.h>

static int is_pow2(uint32_t v)
{
    return v && ((v & (v - 1)) == 0);
}

int ol_ringbuf_init(ol_ringbuf_t *rb, uint8_t *storage, uint32_t capacity)
{
    if (!rb || !storage || !is_pow2(capacity)) {
        return OL_E_INVAL;
    }
    rb->buf      = storage;
    rb->capacity = capacity;
    rb->mask     = capacity - 1;
    rb->head     = 0;
    rb->tail     = 0;
    return OL_OK;
}

uint32_t ol_ringbuf_used(const ol_ringbuf_t *rb)
{
    return rb->head - rb->tail;
}

uint32_t ol_ringbuf_free(const ol_ringbuf_t *rb)
{
    return rb->capacity - (rb->head - rb->tail);
}

bool ol_ringbuf_empty(const ol_ringbuf_t *rb)
{
    return rb->head == rb->tail;
}

bool ol_ringbuf_full(const ol_ringbuf_t *rb)
{
    return (rb->head - rb->tail) == rb->capacity;
}

void ol_ringbuf_reset(ol_ringbuf_t *rb)
{
    rb->head = rb->tail = 0;
}

uint32_t ol_ringbuf_write(ol_ringbuf_t *rb, const uint8_t *data, uint32_t len)
{
    uint32_t free_n = ol_ringbuf_free(rb);
    if (len > free_n) {
        len = free_n;
    }
    uint32_t head_pos = rb->head & rb->mask;
    uint32_t first    = rb->capacity - head_pos;
    if (first > len) {
        first = len;
    }
    memcpy(rb->buf + head_pos, data, first);
    if (len > first) {
        memcpy(rb->buf, data + first, len - first);
    }
    rb->head += len;
    return len;
}

uint32_t ol_ringbuf_read(ol_ringbuf_t *rb, uint8_t *data, uint32_t len)
{
    uint32_t used = ol_ringbuf_used(rb);
    if (len > used) {
        len = used;
    }
    uint32_t tail_pos = rb->tail & rb->mask;
    uint32_t first    = rb->capacity - tail_pos;
    if (first > len) {
        first = len;
    }
    memcpy(data, rb->buf + tail_pos, first);
    if (len > first) {
        memcpy(data + first, rb->buf, len - first);
    }
    rb->tail += len;
    return len;
}
