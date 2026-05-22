#ifndef __AT_CLIENT_H__
#define __AT_CLIENT_H__

#include <stdint.h>
#include <stdbool.h>

/* AT客户端状态 */
typedef enum {
    AT_STATUS_OK = 0,
    AT_STATUS_ERROR,
    AT_STATUS_TIMEOUT,
    AT_STATUS_BUSY
} at_status_t;

/* AT响应结构 */
typedef struct {
    char *buffer;
    uint16_t buffer_size;
    uint16_t data_len;
    uint32_t timeout;
} at_response_t;

/* AT客户端结构 */
typedef struct {
    uint8_t uart_dev;           /* 使用的UART设备 */
    char cmd_buffer[256];       /* 命令缓冲区 */
    char resp_buffer[1024];     /* 响应缓冲区 */
    bool busy;                  /* 忙标志 */
} at_client_t;

/* AT客户端API */
bool at_client_init(at_client_t *client, uint8_t uart_dev);
at_status_t at_send_cmd(at_client_t *client, const char *cmd, const char *expect, uint32_t timeout);
at_status_t at_send_cmd_get_resp(at_client_t *client, const char *cmd, 
                                 char *resp_buf, uint16_t buf_size, uint32_t timeout);
void at_client_flush(at_client_t *client);

/* 响应解析辅助函数 */
bool at_resp_find_line(const char *resp, const char *keyword, char *line, uint16_t line_size);
bool at_resp_parse_line_args(const char *line, const char *format, ...);

#endif /* __AT_CLIENT_H__ */