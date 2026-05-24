/*
 * OpenLoad - YMODEM (Batch) Receiver
 *
 * 协议: YMODEM = XMODEM-CRC 帧 + 文件信息 block 0.
 *
 *   接收方                              发送方
 *     'C'  (启动 CRC)            →
 *                                   ← SOH 0 ~0 <filename\0size dec...\0pad> crc16
 *     ACK + 'C'                  →
 *                                   ← SOH/STX 1 ~1 <data> crc16
 *     ACK                        →   ...
 *                                   ← EOT
 *     NAK                        →
 *                                   ← EOT
 *     ACK + 'C'                  →
 *                                   ← SOH 0 ~0 <0...> crc16     (批次结束)
 *     ACK                        →
 *
 * 状态机:
 *   WAIT_HEADER     已发 'C', 等 block 0
 *   WAIT_DATA       已 ACK header + 再发 'C', 接收数据帧 seq=1..N
 *   WAIT_FINAL_HDR  EOT 双确认 + 'C' 已发, 等末尾空 block 0
 *
 * 与 XMODEM 共享 CRC16 算法与控制字符语义, 但实现独立, 不跨模块 include —
 * 这样在 OPENLOAD_ENABLE_XMODEM=0 时 YMODEM 仍可单独构建.
 */
#include "openload/proto/ymodem.h"
#include "openload/receiver.h"
#include "openload/partition.h"
#include "openload/logger.h"
#include "openload/errno.h"
#include "openload/config.h"
#include "openload/ops/io_ops.h"
#include <string.h>
#include <stddef.h>

/* XMODEM 系协议控制字符 (本文件局部副本, 避免链接级硬依赖 xmodem 模块) */
#define YM_SOH   0x01
#define YM_STX   0x02
#define YM_EOT   0x04
#define YM_ACK   0x06
#define YM_NAK   0x15
#define YM_CAN   0x18
#define YM_C     0x43   /* 'C' */

#define OL_YMODEM_TIMEOUT_MS        1500
#define OL_YMODEM_START_RETRY       10
#define OL_YMODEM_MAX_RETRY         10
#define OL_YMODEM_FINAL_HDR_RETRY   3      /* 末尾 block 0 缺失时的宽限次数 */

typedef enum {
    YM_S_WAIT_HEADER = 0,
    YM_S_WAIT_DATA,
    YM_S_WAIT_FINAL_HDR,
} ym_state_t;

typedef struct {
    const ol_partition_t *dst;
    ol_io_dev_t          *io;
    uint32_t              write_off;
    uint32_t              file_size;       /* 由 block 0 解析得到, 仅用于进度/日志 */
    uint8_t               next_seq;        /* 期望的数据帧 seq, 从 1 开始 */
    uint8_t               start_retry;     /* WAIT_HEADER/WAIT_FINAL_HDR 期间 'C' 重发计数 */
    uint8_t               retry;           /* 单帧 CRC 错误重传计数 */
    ym_state_t            state;
    uint8_t               frame[1024];     /* SOH 128 / STX 1024 共用 */
} ol_ymodem_priv_t;

static ol_ymodem_priv_t g_ymodem_priv;

/* CRC16-CCITT XMODEM 变体, 多项式 0x1021, init 0, 不反射. */
static uint16_t ymodem_crc(const uint8_t *data, uint32_t len)
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

static inline int ym_read_byte(ol_ymodem_priv_t *p, uint8_t *c, uint32_t to)
{
    return ol_io_getc_timeout(p->io, c, to);
}
static inline void ym_send_byte(ol_ymodem_priv_t *p, uint8_t c)
{
    ol_io_putc(p->io, c);
}

/* 解析 block 0: data = "<filename>\0<size_dec>[ <modtime>[...]]\0pad..."
 * out_size 写解析到的 size; 文件名为空 (data[0]==0) 时不修改. */
static void ym_parse_header(const uint8_t *data, uint32_t *out_size)
{
    *out_size = 0;
    if (data[0] == 0) { return; }                /* 空 header = 批次结束 */
    uint32_t i = 0;
    while (i < 128 && data[i]) { ++i; }
    if (i >= 128) { return; }
    ++i;
    uint32_t v = 0;
    while (i < 128 && data[i] >= '0' && data[i] <= '9') {
        v = v * 10 + (uint32_t)(data[i] - '0');
        ++i;
    }
    *out_size = v;
}

static int ym_begin(ol_receiver_t *r, ol_io_dev_t *io,
                    const ol_partition_t *dst)
{
    if (!io || !dst) { return OL_E_INVAL; }
    ol_ymodem_priv_t *p = (ol_ymodem_priv_t *)r->priv;
    p->dst         = dst;
    p->io          = io;
    p->write_off   = 0;
    p->file_size   = 0;
    p->next_seq    = 1;
    p->start_retry = 0;
    p->retry       = 0;
    p->state       = YM_S_WAIT_HEADER;
    ym_send_byte(p, YM_C);
    return OL_OK;
}

/* 收一帧通用入口.
 * 返回:
 *   OL_OK     有效数据帧 ready: *seq_out / *data_len 已填好, p->frame 含 payload
 *   OL_OK + 1 完整 EOT 已双确认 (单文件结束)
 *   OL_E_BUSY 噪声 / 启动期重发 / 帧 CRC 错重 NAK 等 — 调用者继续 poll
 *   其它 <0   硬错误
 *
 * 在 WAIT_HEADER 期间超时会周期重发 'C'; 在 WAIT_FINAL_HDR 期间超时几次后
 * 视为发送端不发末尾空 block 0, 直接返回 OL_OK+1 表示完成.
 */
static int ym_recv_frame(ol_ymodem_priv_t *p, uint8_t *seq_out,
                         uint32_t *data_len)
{
    uint8_t  c;
    uint32_t to = (p->state == YM_S_WAIT_HEADER)
                      ? OL_YMODEM_TIMEOUT_MS
                      : OL_YMODEM_TIMEOUT_MS * 4;
    int rc = ym_read_byte(p, &c, to);
    if (rc != OL_OK) {
        if (p->state == YM_S_WAIT_HEADER) {
            if (++p->start_retry >= OL_YMODEM_START_RETRY) { return OL_E_TIMEOUT; }
            ym_send_byte(p, YM_C);
            return OL_E_BUSY;
        }
        if (p->state == YM_S_WAIT_FINAL_HDR) {
            if (++p->start_retry >= OL_YMODEM_FINAL_HDR_RETRY) {
                return OL_OK + 1;       /* 末尾 header 缺失, 视为完成 */
            }
            ym_send_byte(p, YM_C);
            return OL_E_BUSY;
        }
        return OL_E_TIMEOUT;
    }

    uint32_t dlen = 0;
    if (c == YM_SOH) {
        dlen = 128;
    } else if (c == YM_STX) {
        dlen = 1024;
    } else if (c == YM_EOT) {
        ym_send_byte(p, YM_NAK);
        rc = ym_read_byte(p, &c, OL_YMODEM_TIMEOUT_MS);
        if (rc == OL_OK && c == YM_EOT) {
            ym_send_byte(p, YM_ACK);
            return OL_OK + 1;
        }
        return OL_E_RX_PROTOCOL;
    } else if (c == YM_CAN) {
        if (ym_read_byte(p, &c, OL_YMODEM_TIMEOUT_MS) == OL_OK && c == YM_CAN) {
            return OL_E_RX_CANCELED;
        }
        return OL_E_BUSY;
    } else {
        return OL_E_BUSY;     /* 噪声字节, 继续等 */
    }

    uint8_t seq, seq_inv;
    if (ym_read_byte(p, &seq, OL_YMODEM_TIMEOUT_MS) != OL_OK ||
        ym_read_byte(p, &seq_inv, OL_YMODEM_TIMEOUT_MS) != OL_OK) {
        return OL_E_TIMEOUT;
    }
    for (uint32_t i = 0; i < dlen; ++i) {
        if (ym_read_byte(p, &p->frame[i], OL_YMODEM_TIMEOUT_MS) != OL_OK) {
            return OL_E_TIMEOUT;
        }
    }
    uint8_t crc_hi, crc_lo;
    if (ym_read_byte(p, &crc_hi, OL_YMODEM_TIMEOUT_MS) != OL_OK ||
        ym_read_byte(p, &crc_lo, OL_YMODEM_TIMEOUT_MS) != OL_OK) {
        return OL_E_TIMEOUT;
    }

    if ((seq ^ seq_inv) != 0xFF) {
        ym_send_byte(p, YM_NAK);
        return OL_E_BUSY;
    }
    uint16_t want = ((uint16_t)crc_hi << 8) | crc_lo;
    if (ymodem_crc(p->frame, dlen) != want) {
        if (++p->retry >= OL_YMODEM_MAX_RETRY) {
            ym_send_byte(p, YM_CAN);
            ym_send_byte(p, YM_CAN);
            return OL_E_RX_CRC;
        }
        ym_send_byte(p, YM_NAK);
        return OL_E_BUSY;
    }

    p->retry  = 0;
    *seq_out  = seq;
    *data_len = dlen;
    return OL_OK;
}

static int ym_poll(ol_receiver_t *r)
{
    ol_ymodem_priv_t *p = (ol_ymodem_priv_t *)r->priv;
    uint8_t  seq      = 0;
    uint32_t data_len = 0;
    int rc = ym_recv_frame(p, &seq, &data_len);
    if (rc == OL_E_BUSY) { return 0; }

    switch (p->state) {
    case YM_S_WAIT_HEADER:
        if (rc != OL_OK) { return rc; }
        if (seq != 0) {
            ym_send_byte(p, YM_CAN);
            ym_send_byte(p, YM_CAN);
            return OL_E_RX_PROTOCOL;
        }
        if (p->frame[0] == 0) {
            /* 极少见: 第一个 header 就是空 block 0, 视为空批次 */
            ym_send_byte(p, YM_ACK);
            return 1;
        }
        ym_parse_header(p->frame, &p->file_size);
        OL_LOGI("ymodem: file=%s size=%u",
                (const char *)p->frame, p->file_size);
        ym_send_byte(p, YM_ACK);
        ym_send_byte(p, YM_C);
        p->state       = YM_S_WAIT_DATA;
        p->start_retry = 0;
        return 0;

    case YM_S_WAIT_DATA:
        if (rc == OL_OK + 1) {
            /* 单文件 EOT 完成, 进入末尾 block 0 等待 */
            ym_send_byte(p, YM_C);
            p->state       = YM_S_WAIT_FINAL_HDR;
            p->start_retry = 0;
            return 0;
        }
        if (rc != OL_OK) { return rc; }
        if (seq == (uint8_t)(p->next_seq - 1)) {
            ym_send_byte(p, YM_ACK);             /* 重复帧 */
            return 0;
        }
        if (seq != p->next_seq) {
            ym_send_byte(p, YM_CAN);
            ym_send_byte(p, YM_CAN);
            return OL_E_RX_PROTOCOL;
        }
        {
            int wrc = ol_part_write(p->dst, p->write_off, p->frame, data_len);
            if (wrc != OL_OK) {
                OL_LOGE("flash write failed at off=%u: %s",
                        p->write_off, ol_strerror(wrc));
                ym_send_byte(p, YM_CAN);
                ym_send_byte(p, YM_CAN);
                return wrc;
            }
        }
        p->write_off += data_len;
        p->next_seq++;
        ym_send_byte(p, YM_ACK);
        return 0;

    case YM_S_WAIT_FINAL_HDR:
        if (rc == OL_OK + 1) { return 1; }       /* 退化路径, 末尾 header 超时 */
        if (rc != OL_OK)     { return rc; }
        /* 末尾 block 0 收到 (frame[0] 通常为 0), 无条件 ACK 结束批次 */
        ym_send_byte(p, YM_ACK);
        return 1;

    default:
        return OL_E_RX_PROTOCOL;
    }
}

static int ym_end(ol_receiver_t *r)
{
    (void)r;
    return OL_OK;
}

static uint8_t ym_progress(ol_receiver_t *r)
{
    ol_ymodem_priv_t *p = (ol_ymodem_priv_t *)r->priv;
    uint32_t denom = p->file_size ? p->file_size
                                  : (p->dst ? p->dst->size : 0);
    if (!denom) { return 0; }
    uint64_t pct = (uint64_t)p->write_off * 100 / denom;
    return (uint8_t)(pct > 100 ? 100 : pct);
}

static const ol_receiver_ops_t ymodem_ops = {
    .begin    = ym_begin,
    .poll     = ym_poll,
    .end      = ym_end,
    .progress = ym_progress,
};

ol_receiver_t ol_ymodem_receiver = {
    .name = "ymodem",
    .ops  = &ymodem_ops,
    .priv = &g_ymodem_priv,
};

OL_RECEIVER_REGISTER(ymodem, &ol_ymodem_receiver);
