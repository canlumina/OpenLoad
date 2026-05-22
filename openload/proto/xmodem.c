/*
 * OpenLoad - XMODEM / XMODEM-1K Receiver
 *
 * 协议: 经典 XMODEM-CRC + 可选 1K 扩展。
 *
 *   发送方 ─────────────────────  接收方
 *                                  'C' (启动 CRC 模式, 最多重试 N 次)
 *     SOH/STX seq ~seq data crc16  →
 *                                  ACK 或 NAK
 *     EOT                          →
 *                                  NAK (要求确认)
 *     EOT                          →
 *                                  ACK
 *
 * 流式写入: 每收完一帧立刻写入 staging 分区。调用方需先擦干净。
 *
 * 异常处理:
 *   - 单帧 CRC 错: NAK, 同一 seq 等待重发 (最多 OL_XMODEM_MAX_RETRY 次)
 *   - 整体超时: 取消接收, 返回 OL_E_TIMEOUT
 *   - 收到 CAN CAN: 视为发送方取消, 返回 OL_E_RX_CANCELED
 */
#include "openload/proto/xmodem.h"
#include "openload/receiver.h"
#include "openload/partition.h"
#include "openload/logger.h"
#include "openload/errno.h"
#include "openload/config.h"
#include "openload/ops/io_ops.h"
#include "openload/ops/sys_ops.h"
#include <string.h>
#include <stddef.h>

#define OL_XMODEM_TIMEOUT_MS        1500      /* 单字符读取超时 */
#define OL_XMODEM_START_RETRY       10        /* 启动时发 'C' 的次数 */
#define OL_XMODEM_MAX_RETRY         10        /* 同一帧重传上限 */

typedef struct {
    const ol_partition_t *dst;
    ol_io_dev_t          *io;
    uint32_t              write_off;
    uint8_t               next_seq;
    uint8_t               start_retry;
    uint8_t               retry;
    uint8_t               started;             /* 是否已收到第一帧 */
    /* 单帧缓冲: SOH(1) + seq(1) + ~seq(1) + data(1024) + crc(2) */
    uint8_t               frame[1024];
} ol_xmodem_priv_t;

static ol_xmodem_priv_t g_xmodem_priv;

/* CRC16-CCITT (XMODEM 变体, 多项式 0x1021, init 0x0000, no reflection) */
static uint16_t xmodem_crc(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0;
    while (len--) {
        crc ^= ((uint16_t)*data++) << 8;
        for (int i = 0; i < 8; ++i) {
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
        }
    }
    return crc;
}

static int xm_read_byte(ol_xmodem_priv_t *p, uint8_t *c, uint32_t timeout_ms)
{
    return ol_io_getc_timeout(p->io, c, timeout_ms);
}

static void xm_send_byte(ol_xmodem_priv_t *p, uint8_t c)
{
    ol_io_putc(p->io, c);
}

static int xm_begin(ol_receiver_t *r, ol_io_dev_t *io,
                    const ol_partition_t *dst)
{
    ol_xmodem_priv_t *p = (ol_xmodem_priv_t *)r->priv;
    p->dst         = dst;
    p->io          = io;
    p->write_off   = 0;
    p->next_seq    = 1;
    p->start_retry = 0;
    p->retry       = 0;
    p->started     = 0;
    if (!io || !dst) { return OL_E_INVAL; }
    /* 先吐一个 'C' 启动 CRC 握手 */
    xm_send_byte(p, XMODEM_C);
    return OL_OK;
}

/* 接收一帧: 期待 SOH / STX / EOT / CAN. 阻塞至超时. */
static int xm_recv_frame(ol_xmodem_priv_t *p, uint32_t *data_len_out)
{
    uint8_t  c;
    uint32_t to = p->started ? OL_XMODEM_TIMEOUT_MS * 4 : OL_XMODEM_TIMEOUT_MS;
    int      rc = xm_read_byte(p, &c, to);
    if (rc != OL_OK) {
        /* 启动期: 周期重发 'C' */
        if (!p->started) {
            if (++p->start_retry >= OL_XMODEM_START_RETRY) {
                return OL_E_TIMEOUT;
            }
            xm_send_byte(p, XMODEM_C);
            return OL_E_BUSY;       /* 让 poll 再来一轮 */
        }
        return OL_E_TIMEOUT;
    }

    uint32_t data_len = 0;
    if (c == XMODEM_SOH) {
        data_len = 128;
    } else if (c == XMODEM_STX) {
#if OPENLOAD_ENABLE_XMODEM_1K
        data_len = 1024;
#else
        xm_send_byte(p, XMODEM_NAK);
        return OL_E_BUSY;
#endif
    } else if (c == XMODEM_EOT) {
        /* 第一次回 NAK 要求确认, 第二次回 ACK */
        xm_send_byte(p, XMODEM_NAK);
        rc = xm_read_byte(p, &c, OL_XMODEM_TIMEOUT_MS);
        if (rc == OL_OK && c == XMODEM_EOT) {
            xm_send_byte(p, XMODEM_ACK);
            return OL_OK + 1;       /* 用 +1 表示传输完成 */
        }
        return OL_E_RX_PROTOCOL;
    } else if (c == XMODEM_CAN) {
        if (xm_read_byte(p, &c, OL_XMODEM_TIMEOUT_MS) == OL_OK && c == XMODEM_CAN) {
            return OL_E_RX_CANCELED;
        }
        return OL_E_BUSY;
    } else {
        /* 噪声字节, 继续等 */
        return OL_E_BUSY;
    }

    /* 读 seq + ~seq + data + 2B CRC */
    uint8_t  seq, seq_inv;
    if (xm_read_byte(p, &seq, OL_XMODEM_TIMEOUT_MS) != OL_OK ||
        xm_read_byte(p, &seq_inv, OL_XMODEM_TIMEOUT_MS) != OL_OK) {
        return OL_E_TIMEOUT;
    }
    for (uint32_t i = 0; i < data_len; ++i) {
        if (xm_read_byte(p, &p->frame[i], OL_XMODEM_TIMEOUT_MS) != OL_OK) {
            return OL_E_TIMEOUT;
        }
    }
    uint8_t crc_hi, crc_lo;
    if (xm_read_byte(p, &crc_hi, OL_XMODEM_TIMEOUT_MS) != OL_OK ||
        xm_read_byte(p, &crc_lo, OL_XMODEM_TIMEOUT_MS) != OL_OK) {
        return OL_E_TIMEOUT;
    }

    if ((seq ^ seq_inv) != 0xFF) {
        xm_send_byte(p, XMODEM_NAK);
        return OL_E_BUSY;
    }
    uint16_t want = ((uint16_t)crc_hi << 8) | crc_lo;
    uint16_t calc = xmodem_crc(p->frame, data_len);
    if (calc != want) {
        if (++p->retry >= OL_XMODEM_MAX_RETRY) {
            xm_send_byte(p, XMODEM_CAN);
            xm_send_byte(p, XMODEM_CAN);
            return OL_E_RX_CRC;
        }
        xm_send_byte(p, XMODEM_NAK);
        return OL_E_BUSY;
    }

    /* 期望 seq = next_seq; 若已收过 (重复) 则直接 ACK 不再写 */
    if (seq == (uint8_t)(p->next_seq - 1)) {
        xm_send_byte(p, XMODEM_ACK);
        return OL_E_BUSY;
    }
    if (seq != p->next_seq) {
        xm_send_byte(p, XMODEM_CAN);
        xm_send_byte(p, XMODEM_CAN);
        return OL_E_RX_PROTOCOL;
    }

    p->started = 1;
    p->retry   = 0;
    *data_len_out = data_len;
    return OL_OK;
}

static int xm_poll(ol_receiver_t *r)
{
    ol_xmodem_priv_t *p = (ol_xmodem_priv_t *)r->priv;
    uint32_t data_len = 0;
    int rc = xm_recv_frame(p, &data_len);
    if (rc == OL_E_BUSY) {
        return 0;       /* 继续轮询 */
    }
    if (rc == OL_OK + 1) {
        return 1;       /* 完成 */
    }
    if (rc != OL_OK) {
        return rc;      /* 错误 */
    }

    /* 把这一帧写入 staging. 不裁剪尾部 0x1A (CPMEOF), 留给上层按 image header
       的 firmware_size 自然忽略多余字节. */
    int wrc = ol_part_write(p->dst, p->write_off, p->frame, data_len);
    if (wrc != OL_OK) {
        OL_LOGE("flash write failed at off=%u: %s", p->write_off, ol_strerror(wrc));
        xm_send_byte(p, XMODEM_CAN);
        xm_send_byte(p, XMODEM_CAN);
        return wrc;
    }
    p->write_off += data_len;
    p->next_seq++;
    xm_send_byte(p, XMODEM_ACK);
    return 0;
}

static int xm_end(ol_receiver_t *r)
{
    (void)r;
    return OL_OK;
}

static uint8_t xm_progress(ol_receiver_t *r)
{
    ol_xmodem_priv_t *p = (ol_xmodem_priv_t *)r->priv;
    if (!p->dst || p->dst->size == 0) { return 0; }
    uint64_t pct = (uint64_t)p->write_off * 100 / p->dst->size;
    return (uint8_t)(pct > 100 ? 100 : pct);
}

static const ol_receiver_ops_t xmodem_ops = {
    .begin    = xm_begin,
    .poll     = xm_poll,
    .end      = xm_end,
    .progress = xm_progress,
};

ol_receiver_t ol_xmodem_receiver = {
    .name = "xmodem",
    .ops  = &xmodem_ops,
    .priv = &g_xmodem_priv,
};

OL_RECEIVER_REGISTER(xmodem, &ol_xmodem_receiver);
