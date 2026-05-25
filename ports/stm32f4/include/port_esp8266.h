/*
 * STM32F4 Port - ESP8266 AT 引擎 (公开接口)
 *
 * 跑在 UART2 (port_uart2.c) 之上, 提供同步 AT 命令收发 + WiFi/TCP 操作.
 * 后续 net io_dev / HTTP OTA 都基于本层堆叠.
 *
 * 设计取舍 (AT v1.2 / v2.x 通用):
 *   - 单连接模式 (CIPMUX=0), 无 link id, 命令/响应都最简
 *   - 同步阻塞 API, 不引入回调; bootloader 单线程, 简单可控
 *   - 数据包模型: tcp_send 阻塞到 ESP 返回 SEND OK; tcp_recv 一次返回一个 +IPD
 *
 * 编译开关: OPENLOAD_ENABLE_ESP8266.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- AT 基础 ---------- */

int port_esp_at_send(const char *cmd, uint32_t timeout_ms,
                     char *resp_buf, uint32_t buf_size);

int port_esp_at_ping(void);

/**
 * 等待 UART2 ringbuf 出现指定子串. 字节会被消费 (用于跳过协议中的握手字节),
 * 不写入 resp_buf. needle 长度 ≤ 32.
 *
 * @return OL_OK / OL_E_TIMEOUT / OL_E_INVAL
 */
int port_esp_wait_str(const char *needle, uint32_t timeout_ms);

/* ---------- WiFi (Station) ---------- */

/**
 * AT+CWMODE=1 + AT+CWJAP="<ssid>","<pass>". 阻塞最多 15s.
 * @return OL_OK / OL_E_AT_ERROR / OL_E_TIMEOUT / OL_E_INVAL
 */
int port_esp_wifi_join(const char *ssid, const char *pass);

/**
 * 拿当前 AP 信息 + 本机 IP (AT+CWJAP? + AT+CIFSR), 拼到 info 缓冲.
 */
int port_esp_wifi_status(char *info, uint32_t size);

/* ---------- TCP ---------- */

/** AT+CIPMUX=0 + AT+CIPSTART="TCP","<host>",<port>. 阻塞最多 8s. */
int port_esp_tcp_open(const char *host, uint16_t port);

/**
 * 走 AT+CIPSEND=<len> + 等 prompt '>' + 发裸数据 + 等 "SEND OK".
 * len ≤ 2048 (ESP8266 单次 CIPSEND 上限).
 */
int port_esp_tcp_send(const uint8_t *data, uint32_t len);

/**
 * 阻塞收一个 +IPD,<n>:<data> 段. n > buf_size 时多余字节丢弃.
 * @return  >= 0 实际写入 buf 的字节数;  < 0 错误码 (OL_E_TIMEOUT / OL_E_RX_PROTOCOL)
 */
int port_esp_tcp_recv(uint8_t *buf, uint32_t buf_size, uint32_t timeout_ms);

int port_esp_tcp_close(void);

/* ---------- net io_dev (M2-12d) ----------
 *
 * 把 TCP 连接封装成 ol_io_dev_t name="net". HTTP OTA / 用户协议层只看到
 * 字节流抽象, 不接触 AT 命令.
 *
 * 调用流: port_esp_net_open() → ol_io_dev_find("net")->ops->read/write
 *         → port_esp_net_close().
 *
 * read() 是非阻塞接口: 没数据立即返回 0; 调用方用 ol_io_read_timeout()
 * 做有界等待. read 内部自动解析 +IPD 段并跨段拼接.
 * write() 阻塞直到 ESP 返回 SEND OK; >2048 字节自动分块为多次 CIPSEND.
 */
int port_esp_net_open(const char *host, uint16_t port);
int port_esp_net_close(void);

#ifdef __cplusplus
}
#endif
