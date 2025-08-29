#ifndef __WIFI_H__
#define __WIFI_H__

#include <stdint.h>
#include <stdbool.h>

// WiFi配置
#define WIFI_SSID               "YANG"
#define WIFI_PASSWORD           "yang123456789"
#define WIFI_CONNECT_TIMEOUT    10000  // 10秒连接超时
#define WIFI_CMD_TIMEOUT        3000   // 3秒命令超时

// WiFi状态
typedef enum {
    WIFI_STATE_IDLE = 0,
    WIFI_STATE_READY,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_ERROR
} wifi_state_t;

// API函数
bool wifi_init(void);
bool wifi_connect(const char *ssid, const char *password);
bool wifi_disconnect(void);
bool wifi_is_connected(void);
wifi_state_t wifi_get_state(void);
bool wifi_reset(void);

// TCP/IP函数
bool wifi_tcp_connect(const char *ip, uint16_t port);
bool wifi_tcp_disconnect(void);
int32_t wifi_tcp_send(const uint8_t *data, uint32_t len);
int32_t wifi_tcp_recv(uint8_t *buffer, uint32_t max_len, uint32_t timeout);

#endif /* __WIFI_H__ */