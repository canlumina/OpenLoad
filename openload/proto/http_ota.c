/*
 * OpenLoad - HTTP OTA Receiver (M2)
 *
 * 状态机:
 *   IDLE          → 等 prepare/begin
 *   PARSE_HEAD    → 逐字节读, 按 \r\n 切行, 解析 status / Content-Length,
 *                   空行后切到 RECV_BODY
 *   RECV_BODY     → 流式 io->read → ol_part_write, 累计 == Content-Length 后完成
 *   DONE          → poll 返回 1
 *
 * 网络栈解耦: 不直接 include port_esp8266.h, 而是通过 ol_io_dev_find("net")
 * 拿到的 io 调 io->ops->connect/disconnect (M2-11 给 ol_io_ops_t 加的字段).
 */
#include "openload/proto/http_ota.h"
#include "openload/receiver.h"
#include "openload/partition.h"
#include "openload/logger.h"
#include "openload/errno.h"
#include "openload/config.h"
#include "openload/ops/io_ops.h"
#include "openload/ops/sys_ops.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>

typedef enum {
    HS_IDLE = 0,
    HS_PARSE_HEAD,
    HS_RECV_BODY,
    HS_DONE,
} http_state_t;

typedef struct {
    char     host[64];
    uint16_t port;
    char     path[160];
    int      url_ready;

    const ol_partition_t *dst;
    ol_io_dev_t          *io;
    http_state_t          state;

    uint32_t  body_total;       /* Content-Length, 0 = 未知 */
    uint32_t  body_received;
    uint32_t  write_off;
    int       status_code;
    int       net_opened;

    uint8_t   line_buf[256];    /* HTTP 头单行 buffer */
    uint32_t  line_len;
} http_ota_priv_t;

static http_ota_priv_t g_http_priv;

/* ---------- 工具 ---------- */

static int ch_lower(int c)
{
    return (c >= 'A' && c <= 'Z') ? (c + 32) : c;
}

/* 大小写不敏感的前缀匹配: line 以 prefix 开头? */
static int header_starts_with_ci(const char *line, const char *prefix)
{
    while (*prefix) {
        if (ch_lower((unsigned char)*line) != ch_lower((unsigned char)*prefix)) {
            return 0;
        }
        ++line; ++prefix;
    }
    return 1;
}

/* 解析 http://host[:port][/path] */
static int parse_url(const char *url, char *host, uint16_t *port_out, char *path)
{
    if (strncmp(url, "http://", 7) != 0) { return OL_E_INVAL; }
    const char *p = url + 7;
    if (!*p) { return OL_E_INVAL; }

    /* 找 ':' (port 起点) 或 '/' (path 起点) */
    const char *colon = NULL;
    const char *slash = NULL;
    for (const char *q = p; *q; ++q) {
        if (*q == ':' && !colon && !slash) { colon = q; }
        if (*q == '/') { slash = q; break; }
    }

    const char *host_end = colon ? colon : (slash ? slash : (p + strlen(p)));
    size_t host_len = (size_t)(host_end - p);
    if (host_len == 0 || host_len >= 64) { return OL_E_INVAL; }
    memcpy(host, p, host_len);
    host[host_len] = 0;

    /* port */
    uint16_t port = 80;
    if (colon) {
        port = 0;
        const char *port_end = slash ? slash : (colon + 1 + strlen(colon + 1));
        for (const char *q = colon + 1; q < port_end; ++q) {
            if (*q < '0' || *q > '9') { return OL_E_INVAL; }
            uint32_t v = (uint32_t)port * 10 + (uint32_t)(*q - '0');
            if (v > 65535) { return OL_E_INVAL; }
            port = (uint16_t)v;
        }
        if (port == 0) { return OL_E_INVAL; }
    }
    *port_out = port;

    /* path */
    if (slash) {
        size_t plen = strlen(slash);
        if (plen >= 160) { return OL_E_INVAL; }
        memcpy(path, slash, plen + 1);
    } else {
        path[0] = '/'; path[1] = 0;
    }
    return OL_OK;
}

/* ---------- receiver ops ---------- */

static int http_prepare(ol_receiver_t *r, const void *arg)
{
    if (!arg) { return OL_E_INVAL; }
    http_ota_priv_t *p = (http_ota_priv_t *)r->priv;
    p->url_ready = 0;
    int rc = parse_url((const char *)arg, p->host, &p->port, p->path);
    if (rc != OL_OK) {
        OL_LOGE("http: bad url");
        return rc;
    }
    OL_LOGI("http: target %s:%u%s", p->host, p->port, p->path);
    p->url_ready = 1;
    return OL_OK;
}

static int http_begin(ol_receiver_t *r, ol_io_dev_t *io_unused,
                      const ol_partition_t *dst)
{
    (void)io_unused;
    http_ota_priv_t *p = (http_ota_priv_t *)r->priv;
    if (!p->url_ready) {
        OL_LOGE("http: url not set (need prepare)");
        return OL_E_INVAL;
    }

    /* HTTP receiver 固定走 "net" io_dev, 不用传入的 console io */
    p->io = ol_io_dev_find("net");
    if (!p->io || !p->io->ops->connect) {
        OL_LOGE("http: net io_dev not available");
        return OL_E_NOT_FOUND;
    }

    p->dst           = dst;
    p->state         = HS_IDLE;
    p->body_total    = 0;
    p->body_received = 0;
    p->write_off     = 0;
    p->line_len      = 0;
    p->status_code   = 0;
    p->net_opened    = 0;

    OL_LOGI("http: TCP connect %s:%u ...", p->host, p->port);
    int rc = p->io->ops->connect(p->io, p->host, p->port);
    if (rc != OL_OK) {
        OL_LOGE("http: connect: %s", ol_strerror(rc));
        return rc;
    }
    p->net_opened = 1;

    /* 发 HTTP/1.0 GET 请求 (1.0 让 server close 端简单) */
    char req[256];
    int n = snprintf(req, sizeof(req),
                     "GET %s HTTP/1.0\r\n"
                     "Host: %s\r\n"
                     "User-Agent: OpenLoad/0.1.0\r\n"
                     "Connection: close\r\n"
                     "\r\n",
                     p->path, p->host);
    if (n <= 0 || (size_t)n >= sizeof(req)) { return OL_E_INVAL; }

    OL_LOGI("http: GET %s", p->path);
    rc = p->io->ops->write(p->io, (const uint8_t *)req, (uint32_t)n);
    if (rc < 0) { return rc; }

    p->state = HS_PARSE_HEAD;
    return OL_OK;
}

static int parse_header_line(http_ota_priv_t *p)
{
    /* 复用 line_buf 末尾的 \r\n 已被 caller 剥掉. */
    const char *line = (const char *)p->line_buf;

    if (p->status_code == 0) {
        /* 首行: "HTTP/1.x XXX YYY" */
        const char *sp = strchr(line, ' ');
        if (!sp) { return OL_E_RX_PROTOCOL; }
        while (*sp == ' ') { ++sp; }
        int code = 0;
        while (*sp >= '0' && *sp <= '9') {
            code = code * 10 + (*sp - '0');
            ++sp;
        }
        p->status_code = code;
        OL_LOGI("http: status %d", code);
        if (code < 200 || code >= 300) { return OL_E_NET_HTTP; }
        return OL_OK;
    }

    if (p->line_buf[0] == 0) {
        /* 空行: 头部结束 */
        p->state = HS_RECV_BODY;
        OL_LOGI("http: body %u bytes (0=unknown)", (unsigned)p->body_total);
        return OL_OK;
    }

    /* 普通 header. 只关心 Content-Length. */
    if (header_starts_with_ci(line, "Content-Length:")) {
        const char *v = line + (int)strlen("Content-Length:");
        while (*v == ' ' || *v == '\t') { ++v; }
        uint32_t n = 0;
        while (*v >= '0' && *v <= '9') {
            n = n * 10 + (uint32_t)(*v - '0');
            ++v;
        }
        p->body_total = n;
    }
    return OL_OK;
}

static int http_poll(ol_receiver_t *r)
{
    http_ota_priv_t *p = (http_ota_priv_t *)r->priv;
    if (p->state == HS_DONE) { return 1; }
    if (p->state == HS_IDLE) { return OL_E_INVAL; }

    if (p->state == HS_PARSE_HEAD) {
        /* 逐字节读, 在 \r\n 边界解析一行 */
        uint8_t c;
        int n = p->io->ops->read(p->io, &c, 1);
        if (n <= 0) { return 0; }

        if (p->line_len < sizeof(p->line_buf) - 1) {
            p->line_buf[p->line_len++] = c;
        } else {
            /* 头行过长, 协议异常 */
            return OL_E_RX_OVERFLOW;
        }
        if (p->line_len >= 2 &&
            p->line_buf[p->line_len - 2] == '\r' &&
            p->line_buf[p->line_len - 1] == '\n') {
            p->line_buf[p->line_len - 2] = 0;
            int rc = parse_header_line(p);
            p->line_len = 0;
            if (rc != OL_OK) { return rc; }
        }
        return 0;
    }

    /* HS_RECV_BODY */
    uint8_t buf[256];
    int n = p->io->ops->read(p->io, buf, sizeof(buf));
    if (n < 0) { return n; }
    if (n == 0) {
        /* 暂时无数据. 如果 body 已经收齐, 算完成. */
        if (p->body_total && p->body_received >= p->body_total) {
            p->state = HS_DONE;
            return 1;
        }
        return 0;
    }

    int wrc = ol_part_write(p->dst, p->write_off, buf, (uint32_t)n);
    if (wrc != OL_OK) {
        OL_LOGE("http: flash write at off=%u: %s",
                (unsigned)p->write_off, ol_strerror(wrc));
        return wrc;
    }
    p->write_off     += (uint32_t)n;
    p->body_received += (uint32_t)n;

    if (p->body_total && p->body_received >= p->body_total) {
        p->state = HS_DONE;
        OL_LOGI("http: body received %u bytes", (unsigned)p->body_received);
        return 1;
    }
    return 0;
}

static int http_end(ol_receiver_t *r)
{
    http_ota_priv_t *p = (http_ota_priv_t *)r->priv;
    if (p->net_opened && p->io && p->io->ops->disconnect) {
        (void)p->io->ops->disconnect(p->io);
        p->net_opened = 0;
    }
    return OL_OK;
}

static uint8_t http_progress(ol_receiver_t *r)
{
    http_ota_priv_t *p = (http_ota_priv_t *)r->priv;
    if (!p->body_total) { return 0; }
    uint64_t pct = (uint64_t)p->body_received * 100 / p->body_total;
    return (uint8_t)(pct > 100 ? 100 : pct);
}

static const ol_receiver_ops_t http_ota_ops = {
    .begin    = http_begin,
    .poll     = http_poll,
    .end      = http_end,
    .progress = http_progress,
    .prepare  = http_prepare,
};

ol_receiver_t ol_http_ota_receiver = {
    .name = "http",
    .ops  = &http_ota_ops,
    .priv = &g_http_priv,
};

OL_RECEIVER_REGISTER(http_ota, &ol_http_ota_receiver);
