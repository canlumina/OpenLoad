#ifndef __HTTP_CLIENT_H__
#define __HTTP_CLIENT_H__

#include "esp8266_wifi.h"
#include <stdint.h>
#include <stdbool.h>

/* HTTP客户端状态 */
typedef enum {
    HTTP_STATUS_OK = 0,
    HTTP_STATUS_ERROR,
    HTTP_STATUS_TIMEOUT,
    HTTP_STATUS_NO_MEMORY,
    HTTP_STATUS_CONNECT_FAILED
} http_status_t;

/* HTTP响应信息 */
typedef struct {
    int status_code;            /* HTTP状态码 */
    int content_length;         /* 内容长度 */
    bool is_chunked;           /* 是否为分块传输 */
    char content_type[64];      /* 内容类型 */
    
    /* 固件加密信息 */
    bool firmware_encrypted;    /* 固件是否加密 */
    char encryption_algorithm[16]; /* 加密算法 */
    char encryption_password[32];  /* 加密密码 */
    
    /* 固件元信息 */
    char firmware_version[32];  /* 固件版本 */
    char firmware_filename[64]; /* 固件文件名 */
    uint32_t firmware_size;     /* 固件大小 */
} http_response_info_t;

/* HTTP客户端会话 */
typedef struct {
    esp8266_device_t *esp8266;     /* ESP8266设备 */
    char host[128];                /* 主机名 */
    uint16_t port;                 /* 端口号 */
    bool connected;                /* TCP连接状态 */
    http_response_info_t response; /* 响应信息 */
    int body_received;             /* 已接收的body数据量 */
    
    /* 数据处理回调函数 */
    int (*data_handler)(uint8_t *data, uint16_t len);
} http_client_t;

/* HTTP客户端API */
bool http_client_init(http_client_t *client, esp8266_device_t *esp8266);
http_status_t http_client_connect(http_client_t *client, const char *host, uint16_t port);
http_status_t http_client_get(http_client_t *client, const char *path);
http_status_t http_client_get_with_range(http_client_t *client, const char *path, 
                                        uint32_t start, uint32_t length);
http_status_t http_client_send_raw_request(http_client_t *client, const char *request);
void http_client_set_data_handler(http_client_t *client, 
                                 int (*handler)(uint8_t *data, uint16_t len));
bool http_client_disconnect(http_client_t *client);

/* 辅助函数 */
bool http_parse_url(const char *url, char *host, uint16_t *port, char *path);

#endif /* __HTTP_CLIENT_H__ */