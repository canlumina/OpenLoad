#ifndef __ESP8266_H__
#define __ESP8266_H__

#include <stdint.h>
#include <stdbool.h>

/* ESP8266配置 */
#define ESP8266_UART_ID         DEV_UART2      /* 使用串口2 */
#define ESP8266_RST_PORT        GPIOE          /* 复位引脚端口 */
#define ESP8266_RST_PIN         GPIO_PIN_9     /* 复位引脚 */

/* 超时配置 */
#define ESP8266_CMD_TIMEOUT     3000            /* 普通命令超时(ms) */
#define ESP8266_WIFI_TIMEOUT    15000           /* WiFi连接超时(ms) */
#define ESP8266_TCP_TIMEOUT     10000           /* TCP连接超时(ms) */

/* 缓冲区大小 */
#define ESP8266_RX_BUF_SIZE     4096            /* 接收缓冲区大小 */
#define ESP8266_TX_BUF_SIZE     1024            /* 发送缓冲区大小 */

/* ESP8266返回码 */
typedef enum {
    ESP_OK = 0,                 /* 成功 */
    ESP_ERROR,                  /* 错误 */
    ESP_TIMEOUT,                /* 超时 */
    ESP_BUSY,                   /* 忙碌 */
    ESP_NO_WIFI,                /* WiFi未连接 */
    ESP_NO_TCP,                 /* TCP未连接 */
    ESP_INVALID_PARAM,          /* 参数无效 */
    ESP_BUFFER_FULL,            /* 缓冲区满 */
} esp_result_t;

/* WiFi连接状态 */
typedef enum {
    WIFI_DISCONNECTED = 0,      /* 未连接 */
    WIFI_CONNECTING,            /* 连接中 */
    WIFI_CONNECTED,             /* 已连接但无IP */
    WIFI_GOT_IP,                /* 已获取IP */
} wifi_status_t;

/* TCP连接状态 */
typedef enum {
    TCP_DISCONNECTED = 0,       /* 未连接 */
    TCP_CONNECTING,             /* 连接中 */
    TCP_CONNECTED,              /* 已连接 */
    TCP_CLOSING,                /* 关闭中 */
} tcp_status_t;

/* WiFi配置结构体 */
typedef struct {
    char ssid[32];              /* WiFi名称 */
    char password[64];          /* WiFi密码 */
    uint8_t auto_connect;       /* 自动重连 */
} wifi_config_t;

/* TCP连接信息 */
typedef struct {
    char host[64];              /* 主机地址 */
    uint16_t port;              /* 端口号 */
    tcp_status_t status;        /* 连接状态 */
} tcp_info_t;

/* HTTP响应信息 */
typedef struct {
    uint16_t status_code;       /* HTTP状态码 */
    uint32_t content_length;    /* 内容长度 */
    bool chunked;               /* 是否分块传输 */
    uint32_t received;          /* 已接收字节数 */
} http_response_t;

/* OTA信息 */
typedef struct {
    char url[256];              /* 固件URL */
    uint32_t total_size;        /* 固件总大小 */
    uint32_t downloaded;        /* 已下载大小 */
    uint8_t progress;           /* 下载进度(0-100) */
} ota_info_t;

/* ==================== 基础功能 ==================== */

/* 初始化和复位 */
esp_result_t esp_init(void);
esp_result_t esp_reset(void);
esp_result_t esp_test(void);
esp_result_t esp_get_version(char *version, uint16_t max_len);

/* AT命令接口 */
esp_result_t esp_send_command(const char *cmd, const char *expect, uint32_t timeout);
esp_result_t esp_send_data(const uint8_t *data, uint16_t len);
uint16_t esp_receive_data(uint8_t *buffer, uint16_t max_len, uint32_t timeout);

/* ==================== WiFi功能 ==================== */

/* WiFi连接管理 */
esp_result_t esp_wifi_connect(const char *ssid, const char *password);
esp_result_t esp_wifi_disconnect(void);
esp_result_t esp_wifi_reconnect(void);
wifi_status_t esp_wifi_get_status(void);
esp_result_t esp_wifi_get_ip(char *ip, uint16_t max_len);
bool esp_wifi_is_connected(void);

/* ==================== TCP功能 ==================== */

/* TCP连接管理 */
esp_result_t esp_tcp_connect(const char *host, uint16_t port);
esp_result_t esp_tcp_disconnect(void);
esp_result_t esp_tcp_send(const uint8_t *data, uint16_t len);
uint16_t esp_tcp_receive(uint8_t *buffer, uint16_t max_len, uint32_t timeout);
tcp_status_t esp_tcp_get_status(void);
bool esp_tcp_is_connected(void);

/* ==================== HTTP功能 ==================== */

/* HTTP客户端 */
esp_result_t esp_http_get(const char *url, http_response_t *response);
uint16_t esp_http_read_data(uint8_t *buffer, uint16_t max_len);
esp_result_t esp_http_close(void);

/* ==================== OTA功能 ==================== */

/* OTA下载 */
esp_result_t esp_ota_start(const char *url, ota_info_t *info);
uint16_t esp_ota_read(uint8_t *buffer, uint16_t max_len);
esp_result_t esp_ota_finish(void);
uint8_t esp_ota_get_progress(void);

#endif /* __ESP8266_H__ */