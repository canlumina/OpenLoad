#include "at_client.h"
#include "dev_usart.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

bool at_client_init(at_client_t *client, uint8_t uart_dev)
{
    if (!client) return false;
    
    client->uart_dev = uart_dev;
    client->busy = false;
    memset(client->cmd_buffer, 0, sizeof(client->cmd_buffer));
    memset(client->resp_buffer, 0, sizeof(client->resp_buffer));
    
    return true;
}

void at_client_flush(at_client_t *client)
{
    if (!client) return;
    
    uint8_t dummy[64];
    /* 清空串口缓冲区 */
    while(uart_read(client->uart_dev, dummy, sizeof(dummy)) > 0) {
        HAL_Delay(1);
    }
}

at_status_t at_send_cmd(at_client_t *client, const char *cmd, const char *expect, uint32_t timeout)
{
    if (!client || !cmd || client->busy) return AT_STATUS_ERROR;
    
    client->busy = true;
    
    /* 清空接收缓冲区 */
    at_client_flush(client);
    
    /* 发送命令 */
    snprintf(client->cmd_buffer, sizeof(client->cmd_buffer), "%s\r\n", cmd);
    uart_write(client->uart_dev, (uint8_t*)client->cmd_buffer, strlen(client->cmd_buffer));
    uart_poll_dma_tx(client->uart_dev);
    
    /* 等待响应 */
    uint32_t start_time = HAL_GetTick();
    uint16_t resp_len = 0;
    bool found_expect = false;
    
    memset(client->resp_buffer, 0, sizeof(client->resp_buffer));
    
    while ((HAL_GetTick() - start_time) < timeout) {
        uint8_t ch;
        if (uart_read(client->uart_dev, &ch, 1) > 0) {
            if (resp_len < (sizeof(client->resp_buffer) - 1)) {
                client->resp_buffer[resp_len++] = ch;
                client->resp_buffer[resp_len] = '\0';
                
                /* 检查是否找到期望的响应 */
                if (expect && strstr(client->resp_buffer, expect)) {
                    found_expect = true;
                    break;
                }
                
                /* 检查错误响应 */
                if (strstr(client->resp_buffer, "ERROR") || strstr(client->resp_buffer, "FAIL")) {
                    client->busy = false;
                    return AT_STATUS_ERROR;
                }
            }
        }
        HAL_Delay(1);
    }
    
    client->busy = false;
    
    if (expect) {
        return found_expect ? AT_STATUS_OK : AT_STATUS_TIMEOUT;
    }
    
    return (resp_len > 0) ? AT_STATUS_OK : AT_STATUS_TIMEOUT;
}

at_status_t at_send_cmd_get_resp(at_client_t *client, const char *cmd, 
                                 char *resp_buf, uint16_t buf_size, uint32_t timeout)
{
    if (!client || !cmd || !resp_buf || client->busy) return AT_STATUS_ERROR;
    
    client->busy = true;
    
    /* 清空接收缓冲区 */
    at_client_flush(client);
    
    /* 发送命令 */
    snprintf(client->cmd_buffer, sizeof(client->cmd_buffer), "%s\r\n", cmd);
    uart_write(client->uart_dev, (uint8_t*)client->cmd_buffer, strlen(client->cmd_buffer));
    uart_poll_dma_tx(client->uart_dev);
    
    /* 等待响应 */
    uint32_t start_time = HAL_GetTick();
    uint16_t resp_len = 0;
    
    memset(client->resp_buffer, 0, sizeof(client->resp_buffer));
    
    while ((HAL_GetTick() - start_time) < timeout) {
        uint8_t ch;
        if (uart_read(client->uart_dev, &ch, 1) > 0) {
            if (resp_len < (sizeof(client->resp_buffer) - 1)) {
                client->resp_buffer[resp_len++] = ch;
                client->resp_buffer[resp_len] = '\0';
                
                /* 检查是否接收完成 */
                if (strstr(client->resp_buffer, "OK") || 
                    strstr(client->resp_buffer, "ERROR") ||
                    strstr(client->resp_buffer, "FAIL")) {
                    break;
                }
            }
        }
        HAL_Delay(1);
    }
    
    /* 复制响应到用户缓冲区 */
    uint16_t copy_len = (resp_len < (buf_size - 1)) ? resp_len : (buf_size - 1);
    memcpy(resp_buf, client->resp_buffer, copy_len);
    resp_buf[copy_len] = '\0';
    
    client->busy = false;
    
    if (strstr(client->resp_buffer, "ERROR") || strstr(client->resp_buffer, "FAIL")) {
        return AT_STATUS_ERROR;
    }
    
    return (resp_len > 0) ? AT_STATUS_OK : AT_STATUS_TIMEOUT;
}

bool at_resp_find_line(const char *resp, const char *keyword, char *line, uint16_t line_size)
{
    if (!resp || !keyword || !line) return false;
    
    const char *start = strstr(resp, keyword);
    if (!start) return false;
    
    /* 找到行的开始 */
    while (start > resp && *(start - 1) != '\n' && *(start - 1) != '\r') {
        start--;
    }
    
    /* 找到行的结束 */
    const char *end = start;
    while (*end && *end != '\r' && *end != '\n') {
        end++;
    }
    
    /* 复制行内容 */
    uint16_t len = end - start;
    if (len >= line_size) len = line_size - 1;
    
    memcpy(line, start, len);
    line[len] = '\0';
    
    return true;
}

bool at_resp_parse_line_args(const char *line, const char *format, ...)
{
    if (!line || !format) return false;
    
    va_list args;
    va_start(args, format);
    
    int result = vsscanf(line, format, args);
    
    va_end(args);
    
    return (result > 0);
}