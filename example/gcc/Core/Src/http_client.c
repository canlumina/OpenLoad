#include "http_client.h"
#include "main.h"
#include "bootloader_cmd.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

bool http_client_init(http_client_t *client, esp8266_device_t *esp8266)
{
    if (!client || !esp8266) return false;
    
    client->esp8266 = esp8266;
    client->port = 80;
    client->connected = false;
    client->data_handler = NULL;
    client->body_received = 0;
    
    memset(client->host, 0, sizeof(client->host));
    memset(&client->response, 0, sizeof(client->response));
    
    return true;
}

bool http_parse_url(const char *url, char *host, uint16_t *port, char *path)
{
    if (!url || !host || !port || !path) return false;
    
    const char *start = url;
    
    /* 跳过http:// */
    if (strncmp(url, "http://", 7) == 0) {
        start += 7;
        *port = 80;
    } else if (strncmp(url, "https://", 8) == 0) {
        start += 8;
        *port = 443;
    } else {
        *port = 80;
    }
    
    /* 查找路径分隔符 */
    const char *path_start = strchr(start, '/');
    
    /* 提取主机名和端口 */
    const char *port_start = strchr(start, ':');
    if (port_start && (!path_start || port_start < path_start)) {
        /* 有端口号 */
        int host_len = port_start - start;
        memcpy(host, start, host_len);
        host[host_len] = '\0';
        
        *port = atoi(port_start + 1);
    } else {
        /* 没有端口号 */
        int host_len = path_start ? (path_start - start) : strlen(start);
        memcpy(host, start, host_len);
        host[host_len] = '\0';
    }
    
    /* 提取路径 */
    if (path_start) {
        strcpy(path, path_start);
    } else {
        strcpy(path, "/");
    }
    
    return true;
}

http_status_t http_client_connect(http_client_t *client, const char *host, uint16_t port)
{
    if (!client || !client->esp8266 || !host) return HTTP_STATUS_ERROR;
    
    /* 断开现有连接 */
    if (client->connected) {
        esp8266_tcp_disconnect(client->esp8266);
        client->connected = false;
    }
    
    /* 建立TCP连接 */
    /* 建立TCP连接 */
    
    if (!esp8266_tcp_connect(client->esp8266, host, port)) {
        return HTTP_STATUS_CONNECT_FAILED;
    }
    
    /* TCP连接成功 */
    
    strncpy(client->host, host, sizeof(client->host) - 1);
    client->port = port;
    client->connected = true;
    
    return HTTP_STATUS_OK;
}

static http_status_t http_send_request(http_client_t *client, const char *request)
{
    if (!client || !client->connected || !request) return HTTP_STATUS_ERROR;
    
    int len = strlen(request);
    if (esp8266_tcp_send(client->esp8266, (const uint8_t*)request, len) != len) {
        return HTTP_STATUS_ERROR;
    }
    
    return HTTP_STATUS_OK;
}

static http_status_t http_parse_response_header(http_client_t *client)
{
    char header_buffer[1024];
    char line[256];
    int header_len = 0;
    bool header_complete = false;
    uint32_t start_time = HAL_GetTick();
    
    /* 接收HTTP响应头 */
    while (!header_complete && (HAL_GetTick() - start_time) < 10000) {
        int received = esp8266_tcp_receive(client->esp8266, 
                                          (uint8_t*)header_buffer + header_len,
                                          sizeof(header_buffer) - header_len - 1,
                                          100);
        if (received > 0) {
            header_len += received;
            header_buffer[header_len] = '\0';
            
            /* 检查是否收到完整的头部 */
            if (strstr(header_buffer, "\r\n\r\n")) {
                header_complete = true;
            }
        }
    }
    
    if (!header_complete) {
        return HTTP_STATUS_TIMEOUT;
    }
    
    /* 暂不处理非+IPD格式的body数据，统一由+IPD处理逻辑处理 */
    client->body_received = 0;
    
    /* HTTP头接收完成 */
    
    /* 处理ESP8266的+IPD格式数据 */
    char *ipd_pos = strstr(header_buffer, "+IPD,");
    if (ipd_pos) {
        /* 解析+IPD,xxxx:获取数据长度 */
        char *len_start = ipd_pos + 5;
        char *colon = strchr(len_start, ':');
        if (colon) {
            /* 获取+IPD声明的数据长度 */
            char len_str[16];
            int len_size = colon - len_start;
            if (len_size < sizeof(len_str) && len_size > 0) {
                memcpy(len_str, len_start, len_size);
                len_str[len_size] = '\0';
                int ipd_data_len = atoi(len_str);
                
                /* HTTP响应从冒号后开始 */
                char *http_start = colon + 1;
                int available_data = header_len - (http_start - header_buffer);
                
                /* 确保不超过+IPD声明的长度 */
                if (available_data > ipd_data_len) {
                    available_data = ipd_data_len;
                }
                
                /* 在HTTP响应中查找头部结束 */
                char *http_header_end = NULL;
                for (int i = 0; i <= available_data - 4; i++) {
                    if (http_start[i] == '\r' && http_start[i+1] == '\n' && 
                        http_start[i+2] == '\r' && http_start[i+3] == '\n') {
                        http_header_end = http_start + i + 4;
                        break;
                    }
                }
                
                if (http_header_end) {
                    /* 计算实际可用的HTTP body长度 */
                    int http_body_len = (http_start + available_data) - http_header_end;
                    
                    if (http_body_len > 0 && client->data_handler) {
                        client->data_handler((uint8_t*)http_header_end, http_body_len);
                        client->body_received = http_body_len;
                    }
                } else {
                    /* HTTP头不完整或者全部都是头部数据 */
                    client->body_received = 0;
                }
            }
        }
    }
    
    /* 解析状态行 - 使用处理后的HTTP数据 */
    char *line_start = header_buffer;
    if (ipd_pos) {
        char *colon = strchr(ipd_pos + 5, ':');
        if (colon) {
            line_start = colon + 1;
        }
    }
    char *line_end = strstr(line_start, "\r\n");
    if (line_end) {
        int line_len = line_end - line_start;
        if (line_len < sizeof(line)) {
            memcpy(line, line_start, line_len);
            line[line_len] = '\0';
            
            /* 解析状态码 HTTP/1.1 200 OK */
            if (sscanf(line, "HTTP/1.%*d %d", &client->response.status_code) != 1) {
                client->response.status_code = 0;
            }
        }
    }
    
    /* 解析Content-Length */
    client->response.content_length = -1;
    const char *content_len = strstr(line_start, "Content-Length: ");
    if (content_len) {
        client->response.content_length = atoi(content_len + 16);
    } else {
        /* Content-Length not found */
    }
    
    /* 检查是否为分块传输 */
    client->response.is_chunked = (strstr(line_start, "Transfer-Encoding: chunked") != NULL);
    
    /* 解析Content-Type */
    const char *content_type = strstr(line_start, "Content-Type: ");
    if (content_type) {
        content_type += 14;
        const char *type_end = strstr(content_type, "\r\n");
        if (type_end) {
            int type_len = type_end - content_type;
            if (type_len < sizeof(client->response.content_type)) {
                memcpy(client->response.content_type, content_type, type_len);
                client->response.content_type[type_len] = '\0';
            }
        }
    }
    
    return HTTP_STATUS_OK;
}

static http_status_t http_receive_data(http_client_t *client)
{
    uint8_t buffer[512];
    int total_received = 0;
    uint32_t start_time = HAL_GetTick();
    
    /* 开始接收数据 */
    
    /* 如果有Content-Length，按长度接收 */
    if (client->response.content_length > 0) {
        /* 减去已经在header buffer中处理的数据 */
        int remaining = client->response.content_length - client->body_received;
        
        /* 如果所有数据已经在header buffer中处理完了 */
        if (remaining <= 0) {
            return HTTP_STATUS_OK;
        }
        
        /* 按Content-Length接收剩余数据 */
        
        while (remaining > 0 && (HAL_GetTick() - start_time) < 30000) {
            int to_receive = (remaining < sizeof(buffer)) ? remaining : sizeof(buffer);
            int received = esp8266_tcp_receive(client->esp8266, buffer, to_receive, 1000);
            
            if (received > 0) {
                /* 数据接收中 - 打印前几次接收的调试信息 */
                static int recv_count = 0;
                recv_count++;
                if (recv_count <= 3) {
                    /* 通过系统调用打印调试信息 */
                }
                
                if (client->data_handler) {
                    if (client->data_handler(buffer, received) < 0) {
                        return HTTP_STATUS_ERROR;
                    }
                }
                remaining -= received;
                total_received += received;
                start_time = HAL_GetTick(); /* 重置超时，只要有数据就继续 */
            } else if (received == 0) {
                /* 如果连接关闭且我们已经收到一些数据，可能是正常结束 */
                if (total_received > 0) {
                    break;
                }
                HAL_Delay(10);
            } else {
                return HTTP_STATUS_ERROR;
            }
        }
    } else {
        /* 没有Content-Length，接收到连接关闭 */
        while ((HAL_GetTick() - start_time) < 30000) {
            int received = esp8266_tcp_receive(client->esp8266, buffer, sizeof(buffer), 1000);
            
            if (received > 0) {
                if (client->data_handler) {
                    if (client->data_handler(buffer, received) < 0) {
                        return HTTP_STATUS_ERROR;
                    }
                }
                total_received += received;
                start_time = HAL_GetTick();  /* 重置超时 */
            } else if (received == 0) {
                /* 没有数据，可能连接已关闭 */
                break;
            } else {
                return HTTP_STATUS_ERROR;
            }
        }
    }
    
    return HTTP_STATUS_OK;
}

http_status_t http_client_get(http_client_t *client, const char *path)
{
    if (!client || !client->connected || !path) return HTTP_STATUS_ERROR;
    
    char request[512];
    
    /* 构建HTTP GET请求 */
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "\r\n",
             path, client->host);
    
    /* 发送请求 */
    http_status_t status = http_send_request(client, request);
    if (status != HTTP_STATUS_OK) {
        return status;
    }
    
    /* 重置body_received计数器 */
    client->body_received = 0;
    
    /* 解析响应头 */
    status = http_parse_response_header(client);
    if (status != HTTP_STATUS_OK) {
        return status;
    }
    
    /* 检查HTTP状态码 */
    if (client->response.status_code != 200) {
        return HTTP_STATUS_ERROR;
    }
    
    /* 接收数据 */
    status = http_receive_data(client);
    
    return status;
}

http_status_t http_client_get_with_range(http_client_t *client, const char *path, 
                                        uint32_t start, uint32_t length)
{
    if (!client || !client->connected || !path) return HTTP_STATUS_ERROR;
    
    char request[512];
    
    /* 构建HTTP GET请求with Range */
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Range: bytes=%lu-%lu\r\n"
             "Connection: close\r\n"
             "\r\n",
             path, client->host, (unsigned long)start, (unsigned long)(start + length - 1));
    
    /* 发送请求 */
    http_status_t status = http_send_request(client, request);
    if (status != HTTP_STATUS_OK) return status;
    
    /* 解析响应头 */
    status = http_parse_response_header(client);
    if (status != HTTP_STATUS_OK) return status;
    
    /* 检查HTTP状态码 */
    if (client->response.status_code != 206 && client->response.status_code != 200) {
        return HTTP_STATUS_ERROR;
    }
    
    /* 接收数据 */
    return http_receive_data(client);
}

http_status_t http_client_send_raw_request(http_client_t *client, const char *request)
{
    if (!client || !client->connected || !request) return HTTP_STATUS_ERROR;
    
    /* 发送请求 */
    http_status_t status = http_send_request(client, request);
    if (status != HTTP_STATUS_OK) return status;
    
    /* 解析响应头 */
    status = http_parse_response_header(client);
    if (status != HTTP_STATUS_OK) return status;
    
    /* HEAD请求没有body，直接返回 */
    return HTTP_STATUS_OK;
}

void http_client_set_data_handler(http_client_t *client, 
                                 int (*handler)(uint8_t *data, uint16_t len))
{
    if (client) {
        client->data_handler = handler;
    }
}

bool http_client_disconnect(http_client_t *client)
{
    if (!client || !client->connected) return false;
    
    esp8266_tcp_disconnect(client->esp8266);
    client->connected = false;
    
    return true;
}