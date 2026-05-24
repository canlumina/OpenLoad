/*
 * STM32F1 Port - ESP8266 AT 引擎实现
 *
 * 协议格式适配 AT v1.2 / v2.x 单连接模式 (CIPMUX=0):
 *   命令终止符:    "\r\nOK\r\n" / "\r\nERROR\r\n" / "\r\nFAIL\r\n"
 *   CIPSEND prompt: ">"
 *   数据上报:       "+IPD,<len>:<data>"  (无 link id)
 *
 * 终止符检测用滑动尾部 buffer 做后缀匹配, 避免依赖完整缓冲也能定位.
 */
#include "port_esp8266.h"
#include "port_uart2.h"
#include "openload/ops/sys_ops.h"
#include "openload/errno.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define AT_TAIL_LEN     16

static int tail_endswith(const char *tail, int tail_len,
                         const char *suffix, int suffix_len)
{
    if (tail_len < suffix_len) { return 0; }
    return memcmp(tail + tail_len - suffix_len, suffix, suffix_len) == 0;
}

int port_esp_at_send(const char *cmd, uint32_t timeout_ms,
                     char *resp_buf, uint32_t buf_size)
{
    if (!cmd) { return OL_E_INVAL; }
    if (resp_buf && buf_size < 1) { return OL_E_INVAL; }

    port_uart2_rx_flush();

    uint32_t cmd_len = (uint32_t)strlen(cmd);
    if (port_uart2_write((const uint8_t *)cmd, cmd_len) < 0) { return OL_E_IO; }
    if (port_uart2_write((const uint8_t *)"\r\n", 2) < 0)    { return OL_E_IO; }

    uint32_t resp_used = 0;
    if (resp_buf) { resp_buf[0] = 0; }

    char tail[AT_TAIL_LEN] = {0};
    int  tail_len = 0;

    uint32_t start = ol_tick_ms();
    while ((ol_tick_ms() - start) < timeout_ms) {
        uint8_t c;
        int n = port_uart2_read(&c, 1);
        if (n <= 0) { continue; }

        if (resp_buf && (resp_used + 1) < buf_size) {
            resp_buf[resp_used++] = (char)c;
            resp_buf[resp_used]   = 0;
        }

        if (tail_len < AT_TAIL_LEN) {
            tail[tail_len++] = (char)c;
        } else {
            memmove(tail, tail + 1, AT_TAIL_LEN - 1);
            tail[AT_TAIL_LEN - 1] = (char)c;
        }

        if (tail_endswith(tail, tail_len, "\r\nOK\r\n", 6))      { return OL_OK; }
        if (tail_endswith(tail, tail_len, "\r\nERROR\r\n", 9))   { return OL_E_AT_ERROR; }
        if (tail_endswith(tail, tail_len, "\r\nFAIL\r\n", 8))    { return OL_E_AT_ERROR; }
    }
    return OL_E_TIMEOUT;
}

int port_esp_at_ping(void)
{
    return port_esp_at_send("AT", 1000, NULL, 0);
}

int port_esp_wait_str(const char *needle, uint32_t timeout_ms)
{
    if (!needle || !needle[0]) { return OL_E_INVAL; }
    uint32_t nlen = (uint32_t)strlen(needle);
    if (nlen > 32) { return OL_E_INVAL; }

    char tail[32];
    uint32_t tail_len = 0;

    uint32_t start = ol_tick_ms();
    while ((ol_tick_ms() - start) < timeout_ms) {
        uint8_t c;
        if (port_uart2_read(&c, 1) <= 0) { continue; }

        if (tail_len < nlen) {
            tail[tail_len++] = (char)c;
        } else {
            memmove(tail, tail + 1, nlen - 1);
            tail[nlen - 1] = (char)c;
        }

        if (tail_len == nlen && memcmp(tail, needle, nlen) == 0) {
            return OL_OK;
        }
    }
    return OL_E_TIMEOUT;
}

/* ---------- WiFi ---------- */

int port_esp_wifi_join(const char *ssid, const char *pass)
{
    if (!ssid || !pass) { return OL_E_INVAL; }

    /* 切 Station 模式; 已是 1 时也返回 OK, 忽略 ERROR (重复设置) */
    (void)port_esp_at_send("AT+CWMODE=1", 1000, NULL, 0);

    char cmd[128];
    int n = snprintf(cmd, sizeof(cmd),
                     "AT+CWJAP=\"%s\",\"%s\"", ssid, pass);
    if (n <= 0 || (size_t)n >= sizeof(cmd)) { return OL_E_INVAL; }

    /* DHCP + 关联通常 5-12s. v1.2 实测有时到 15s */
    return port_esp_at_send(cmd, 20000, NULL, 0);
}

int port_esp_wifi_status(char *info, uint32_t size)
{
    if (info && size > 0) { info[0] = 0; }

    int rc = port_esp_at_send("AT+CWJAP?", 2000, info, size);
    if (rc != OL_OK) { return rc; }

    if (info && size > 0) {
        uint32_t used = (uint32_t)strlen(info);
        if (used < size) {
            return port_esp_at_send("AT+CIFSR", 2000,
                                    info + used, size - used);
        }
    }
    return port_esp_at_send("AT+CIFSR", 2000, NULL, 0);
}

/* ---------- TCP ---------- */

int port_esp_tcp_open(const char *host, uint16_t port)
{
    if (!host) { return OL_E_INVAL; }

    (void)port_esp_at_send("AT+CIPMUX=0", 1000, NULL, 0);

    char cmd[96];
    int n = snprintf(cmd, sizeof(cmd),
                     "AT+CIPSTART=\"TCP\",\"%s\",%u",
                     host, (unsigned)port);
    if (n <= 0 || (size_t)n >= sizeof(cmd)) { return OL_E_INVAL; }

    return port_esp_at_send(cmd, 8000, NULL, 0);
}

int port_esp_tcp_send(const uint8_t *data, uint32_t len)
{
    if (!data || len == 0 || len > 2048) { return OL_E_INVAL; }

    char cmd[32];
    int n = snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", (unsigned)len);
    if (n <= 0 || (size_t)n >= sizeof(cmd)) { return OL_E_INVAL; }

    port_uart2_rx_flush();
    if (port_uart2_write((const uint8_t *)cmd, (uint32_t)n) < 0) { return OL_E_IO; }
    if (port_uart2_write((const uint8_t *)"\r\n", 2) < 0)        { return OL_E_IO; }

    /* 等 ">" prompt (CIPSEND 进入数据接收模式的标志) */
    if (port_esp_wait_str(">", 3000) != OL_OK) { return OL_E_TIMEOUT; }

    /* 发裸数据 */
    if (port_uart2_write(data, len) < 0) { return OL_E_IO; }

    /* 等 "SEND OK" (ESP 已把数据交给 TCP 栈) */
    if (port_esp_wait_str("SEND OK", 5000) != OL_OK) { return OL_E_TIMEOUT; }
    return OL_OK;
}

int port_esp_tcp_recv(uint8_t *buf, uint32_t buf_size, uint32_t timeout_ms)
{
    /* 等 "+IPD," 段头 */
    if (port_esp_wait_str("+IPD,", timeout_ms) != OL_OK) {
        return OL_E_TIMEOUT;
    }

    /* 读 ASCII 数字直到 ':' 得到 payload 长度 */
    uint32_t total = 0;
    uint32_t start = ol_tick_ms();
    int saw_digit = 0;
    while ((ol_tick_ms() - start) < 1000) {
        uint8_t c;
        if (port_uart2_read(&c, 1) <= 0) { continue; }
        if (c == ':') { break; }
        if (c < '0' || c > '9')  { return OL_E_RX_PROTOCOL; }
        total = total * 10 + (uint32_t)(c - '0');
        saw_digit = 1;
        if (total > 65535) { return OL_E_RX_PROTOCOL; }
    }
    if (!saw_digit) { return OL_E_RX_PROTOCOL; }
    if (total == 0) { return 0; }

    uint32_t toread = (buf && total < buf_size) ? total
                    : (buf ? buf_size : 0);
    uint32_t got = 0;
    start = ol_tick_ms();
    while (got < toread && (ol_tick_ms() - start) < timeout_ms) {
        int n = port_uart2_read(buf + got, toread - got);
        if (n > 0) { got += (uint32_t)n; }
    }

    /* 若 payload 超过 buf_size, 把剩余字节吃掉, 避免污染下一次 +IPD 解析 */
    uint32_t consumed = (buf ? got : 0);
    while (consumed < total && (ol_tick_ms() - start) < (timeout_ms + 1000)) {
        uint8_t junk;
        if (port_uart2_read(&junk, 1) > 0) { consumed++; }
    }
    return (int)got;
}

int port_esp_tcp_close(void)
{
    return port_esp_at_send("AT+CIPCLOSE", 1000, NULL, 0);
}

/* ================================================================
 *  net io_dev (M2-12d) — TCP 之上的 ol_io_ops_t 封装
 * ================================================================ */
#include "openload/ops/io_ops.h"

static struct {
    uint8_t  pending[1024];     /* 单段 +IPD payload 缓存; HTTP OTA 段间透明 */
    uint32_t pending_len;
    uint32_t pending_pos;
    uint8_t  connected;
} s_net;

/* CIPSEND 单次上限 2048 (ESP8266 限制); 大块写按 2KB 分包 */
#define NET_CIPSEND_MAX     2048u

/* 单次 read 内部等下一段 +IPD 的超时 (毫秒). 太短会让连续数据流断点判错,
 * 太长会让流尾的"已经收完"等待变长. HTTP OTA 流式数据通常间隔 << 200ms. */
#define NET_RECV_POLL_MS    200u

static int net_read(ol_io_dev_t *dev, uint8_t *out, uint32_t len)
{
    (void)dev;
    if (!out || len == 0) { return 0; }
    uint32_t total = 0;
    while (total < len) {
        if (s_net.pending_pos < s_net.pending_len) {
            uint32_t avail = s_net.pending_len - s_net.pending_pos;
            uint32_t want  = len - total;
            uint32_t copy  = (avail < want) ? avail : want;
            memcpy(out + total, s_net.pending + s_net.pending_pos, copy);
            s_net.pending_pos += copy;
            total += copy;
            continue;
        }
        if (!s_net.connected) { break; }
        /* 装一段新的 +IPD; 没数据立即返回 (let 外层做 timeout 控制) */
        int got = port_esp_tcp_recv(s_net.pending,
                                    sizeof(s_net.pending),
                                    NET_RECV_POLL_MS);
        if (got <= 0) { break; }
        s_net.pending_len = (uint32_t)got;
        s_net.pending_pos = 0;
    }
    return (int)total;
}

static int net_write(ol_io_dev_t *dev, const uint8_t *buf, uint32_t len)
{
    (void)dev;
    if (!s_net.connected) { return OL_E_NET_NO_LINK; }
    uint32_t off = 0;
    while (off < len) {
        uint32_t chunk = len - off;
        if (chunk > NET_CIPSEND_MAX) { chunk = NET_CIPSEND_MAX; }
        int rc = port_esp_tcp_send(buf + off, chunk);
        if (rc != OL_OK) { return rc; }
        off += chunk;
    }
    return (int)len;
}

static int net_available(ol_io_dev_t *dev)
{
    (void)dev;
    return (int)(s_net.pending_len - s_net.pending_pos);
}

static int net_flush(ol_io_dev_t *dev)
{
    (void)dev;
    return OL_OK;
}

static const ol_io_ops_t s_net_ops = {
    .read      = net_read,
    .write     = net_write,
    .available = net_available,
    .flush     = net_flush,
};

static ol_io_dev_t s_net_dev = {
    .name = "net",
    .ops  = &s_net_ops,
    .priv = 0,
};
OL_IO_DEV_REGISTER(net, &s_net_dev);

int port_esp_net_open(const char *host, uint16_t port)
{
    s_net.connected   = 0;
    s_net.pending_len = 0;
    s_net.pending_pos = 0;
    int rc = port_esp_tcp_open(host, port);
    if (rc != OL_OK) { return rc; }
    s_net.connected = 1;
    return OL_OK;
}

int port_esp_net_close(void)
{
    s_net.connected   = 0;
    s_net.pending_len = 0;
    s_net.pending_pos = 0;
    return port_esp_tcp_close();
}
