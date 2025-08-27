#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp8266.h"
#include "dev_usart.h"
#include "gpio.h"
#include "main.h"

/* 内部缓冲区 */
#define ESP8266_RX_BUF_SIZE     1024
static char esp8266_rx_buffer[ESP8266_RX_BUF_SIZE];

/**
 * @brief ESP8266硬件复位
 */
void esp8266_hw_reset(void)
{
    /* 拉低复位引脚 */
    HAL_GPIO_WritePin(ESP8266_RESET_GPIO_PORT, ESP8266_RESET_GPIO_PIN, GPIO_PIN_RESET);
    HAL_Delay(ESP8266_RESET_DELAY_MS);
    
    /* 拉高复位引脚，释放复位 */
    HAL_GPIO_WritePin(ESP8266_RESET_GPIO_PORT, ESP8266_RESET_GPIO_PIN, GPIO_PIN_SET);
    HAL_Delay(ESP8266_BOOT_DELAY_MS);  /* 等待模块启动 */
}

/**
 * @brief ESP8266硬件初始化
 */
void esp8266_hw_init(void)
{
    /* 初始化串口2 */
    uart_device_init(ESP8266_UART_ID);
    
    /* 配置复位引脚为输出 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOE_CLK_ENABLE();
    
    GPIO_InitStruct.Pin = ESP8266_RESET_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(ESP8266_RESET_GPIO_PORT, &GPIO_InitStruct);
    
    /* 默认拉高复位引脚 */
    HAL_GPIO_WritePin(ESP8266_RESET_GPIO_PORT, ESP8266_RESET_GPIO_PIN, GPIO_PIN_SET);
}

/**
 * @brief 读取ESP8266响应数据
 * @param buffer 接收缓冲区
 * @param max_len 缓冲区最大长度
 * @param timeout_ms 超时时间(毫秒)
 * @retval 实际读取到的字节数
 */
uint16_t esp8266_read_response(char *buffer, uint16_t max_len, uint32_t timeout_ms)
{
    uint32_t start_time = HAL_GetTick();
    uint16_t total_len = 0;
    uint16_t read_len = 0;
    
    memset(buffer, 0, max_len);
    
    while ((HAL_GetTick() - start_time) < timeout_ms && total_len < (max_len - 1))
    {
        /* 轮询串口发送 */
        uart_poll_dma_tx(ESP8266_UART_ID);
        
        /* 读取串口数据 */
        read_len = uart_read(ESP8266_UART_ID, (uint8_t *)&buffer[total_len], max_len - total_len - 1);
        if (read_len > 0)
        {
            total_len += read_len;
            buffer[total_len] = '\0';  /* 确保字符串结尾 */
            
            /* 检查是否收到完整响应 */
            if (strstr(buffer, "OK") || strstr(buffer, "ERROR") || 
                strstr(buffer, "FAIL") || strstr(buffer, ">"))
            {
                break;
            }
        }
        HAL_Delay(1);
    }
    
    return total_len;
}

/**
 * @brief 发送AT命令并等待指定响应
 * @param cmd 要发送的命令
 * @param expected_resp 期望的响应字符串，NULL表示不检查响应
 * @param timeout_ms 超时时间(毫秒)
 * @retval ESP8266状态
 */
esp8266_status_t esp8266_send_cmd(const char *cmd, const char *expected_resp, uint32_t timeout_ms)
{
    uint16_t cmd_len = strlen(cmd);
    uint16_t resp_len = 0;
    
    /* 发送命令 */
    uart_write(ESP8266_UART_ID, (const uint8_t *)cmd, cmd_len);
    uart_write(ESP8266_UART_ID, (const uint8_t *)"\r\n", 2);
    
    /* 轮询发送 */
    uart_poll_dma_tx(ESP8266_UART_ID);
    
    /* 如果不需要检查响应，直接返回成功 */
    if (expected_resp == NULL)
    {
        HAL_Delay(100);  /* 给一点延时 */
        return ESP8266_OK;
    }
    
    /* 读取响应 */
    resp_len = esp8266_read_response(esp8266_rx_buffer, ESP8266_RX_BUF_SIZE, timeout_ms);
    
    if (resp_len == 0)
    {
        return ESP8266_TIMEOUT;
    }
    
    /* 检查响应 */
    if (strstr(esp8266_rx_buffer, expected_resp))
    {
        return ESP8266_OK;
    }
    else if (strstr(esp8266_rx_buffer, "ERROR") || strstr(esp8266_rx_buffer, "FAIL"))
    {
        return ESP8266_ERROR;
    }
    
    return ESP8266_TIMEOUT;
}

/**
 * @brief 发送数据
 * @param data 要发送的数据
 * @param len 数据长度
 * @retval ESP8266状态
 */
esp8266_status_t esp8266_send_data(const uint8_t *data, uint16_t len)
{
    uart_write(ESP8266_UART_ID, data, len);
    uart_poll_dma_tx(ESP8266_UART_ID);
    return ESP8266_OK;
}

/**
 * @brief 测试ESP8266连接
 * @retval ESP8266状态
 */
esp8266_status_t esp8266_test(void)
{
    return esp8266_send_cmd("AT", "OK", ESP8266_CMD_TIMEOUT_MS);
}

/**
 * @brief 重启ESP8266
 * @retval ESP8266状态
 */
esp8266_status_t esp8266_reset(void)
{
    esp8266_status_t status;
    
    status = esp8266_send_cmd("AT+RST", "OK", ESP8266_CMD_TIMEOUT_MS);
    if (status == ESP8266_OK)
    {
        HAL_Delay(2000);  /* 等待重启完成 */
    }
    
    return status;
}

/**
 * @brief 设置ESP8266工作模式
 * @param mode 工作模式
 * @retval ESP8266状态
 */
esp8266_status_t esp8266_set_mode(esp8266_mode_t mode)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CWMODE=%d", mode);
    return esp8266_send_cmd(cmd, "OK", ESP8266_CMD_TIMEOUT_MS);
}

/**
 * @brief 获取ESP8266版本信息
 * @param version 版本信息缓冲区
 * @param max_len 缓冲区最大长度
 * @retval ESP8266状态
 */
esp8266_status_t esp8266_get_version(char *version, uint16_t max_len)
{
    esp8266_status_t status;
    
    status = esp8266_send_cmd("AT+GMR", "OK", ESP8266_CMD_TIMEOUT_MS);
    if (status == ESP8266_OK && version != NULL)
    {
        /* 复制响应到用户缓冲区 */
        strncpy(version, esp8266_rx_buffer, max_len - 1);
        version[max_len - 1] = '\0';
    }
    
    return status;
}

/**
 * @brief 连接WiFi网络
 * @param ssid WiFi名称
 * @param password WiFi密码
 * @retval ESP8266状态
 */
esp8266_status_t esp8266_connect_wifi(const char *ssid, const char *password)
{
    char cmd[128];
    
    if (ssid == NULL)
    {
        return ESP8266_ERROR;
    }
    
    if (password != NULL && strlen(password) > 0)
    {
        snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    }
    else
    {
        snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"\"", ssid);
    }
    
    return esp8266_send_cmd(cmd, "OK", 10000);  /* WiFi连接需要更长时间 */
}

/**
 * @brief 断开WiFi连接
 * @retval ESP8266状态
 */
esp8266_status_t esp8266_disconnect_wifi(void)
{
    return esp8266_send_cmd("AT+CWQAP", "OK", ESP8266_CMD_TIMEOUT_MS);
}

/**
 * @brief 获取IP地址
 * @param ip IP地址缓冲区
 * @param max_len 缓冲区最大长度
 * @retval ESP8266状态
 */
esp8266_status_t esp8266_get_ip(char *ip, uint16_t max_len)
{
    esp8266_status_t status;
    char *ip_start, *ip_end;
    
    status = esp8266_send_cmd("AT+CIFSR", "OK", ESP8266_CMD_TIMEOUT_MS);
    if (status == ESP8266_OK && ip != NULL)
    {
        /* 查找IP地址 */
        ip_start = strstr(esp8266_rx_buffer, "STAIP,\"");
        if (ip_start != NULL)
        {
            ip_start += 7;  /* 跳过"STAIP," */
            ip_end = strchr(ip_start, '\"');
            if (ip_end != NULL)
            {
                int ip_len = ip_end - ip_start;
                if (ip_len < max_len)
                {
                    strncpy(ip, ip_start, ip_len);
                    ip[ip_len] = '\0';
                    return ESP8266_OK;
                }
            }
        }
        
        /* 如果没找到IP，复制整个响应 */
        strncpy(ip, esp8266_rx_buffer, max_len - 1);
        ip[max_len - 1] = '\0';
    }
    
    return status;
}

/**
 * @brief ESP8266初始化
 * @retval ESP8266状态
 */
esp8266_status_t esp8266_init(void)
{
    esp8266_status_t status;
    
    /* 硬件初始化 */
    esp8266_hw_init();
    
    /* 硬件复位 */
    esp8266_hw_reset();
    
    /* 测试AT命令 */
    status = esp8266_test();
    if (status != ESP8266_OK)
    {
        return status;
    }
    
    /* 设置为Station模式 */
    status = esp8266_set_mode(ESP8266_MODE_STA);
    if (status != ESP8266_OK)
    {
        return status;
    }
    
    /* 关闭回显 */
    status = esp8266_send_cmd("ATE0", "OK", ESP8266_CMD_TIMEOUT_MS);
    
    return status;
}

/**
 * @brief 检查ESP8266是否已连接WiFi
 * @retval true: 已连接, false: 未连接
 */
bool esp8266_is_connected(void)
{
    esp8266_status_t status = esp8266_send_cmd("AT+CWJAP?", "OK", ESP8266_CMD_TIMEOUT_MS);
    
    if (status == ESP8266_OK)
    {
        /* 检查响应中是否包含WiFi信息 */
        return (strstr(esp8266_rx_buffer, "+CWJAP:") != NULL);
    }
    
    return false;
}

/**
 * @brief 获取ESP8266连接状态
 * @retval 连接状态
 */
esp8266_conn_status_t esp8266_get_connection_status(void)
{
    esp8266_status_t status;
    
    /* 检查WiFi连接状态 */
    status = esp8266_send_cmd("AT+CIPSTATUS", "OK", ESP8266_CMD_TIMEOUT_MS);
    
    if (status == ESP8266_OK)
    {
        if (strstr(esp8266_rx_buffer, "STATUS:2"))
        {
            return ESP8266_GOT_IP;
        }
        else if (strstr(esp8266_rx_buffer, "STATUS:3"))
        {
            return ESP8266_CONNECTED;
        }
    }
    
    return ESP8266_DISCONNECTED;
}

/* TCP连接管理 */
static bool tcp_connected = false;

/**
 * @brief 建立TCP连接
 * @param host 主机名或IP地址
 * @param port 端口号
 * @retval ESP8266状态
 */
esp8266_status_t esp8266_tcp_connect(const char *host, uint16_t port)
{
    char cmd[128];
    esp8266_status_t status;
    
    /* 设置单连接模式 */
    status = esp8266_send_cmd("AT+CIPMUX=0", "OK", ESP8266_CMD_TIMEOUT_MS);
    if (status != ESP8266_OK) {
        return status;
    }
    
    /* 建立TCP连接 */
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d", host, port);
    status = esp8266_send_cmd(cmd, "OK", 10000);  /* TCP连接需要更长时间 */
    
    if (status == ESP8266_OK) {
        tcp_connected = true;
    }
    
    return status;
}

/**
 * @brief 发送TCP数据
 * @param data 要发送的数据
 * @param len 数据长度
 * @retval ESP8266状态
 */
esp8266_status_t esp8266_tcp_send(const uint8_t *data, uint16_t len)
{
    char cmd[32];
    esp8266_status_t status;
    
    if (!tcp_connected) {
        return ESP8266_ERROR;
    }
    
    /* 准备发送数据 */
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", len);
    status = esp8266_send_cmd(cmd, ">", 3000);
    if (status != ESP8266_OK) {
        return status;
    }
    
    /* 发送实际数据 */
    uart_write(ESP8266_UART_ID, data, len);
    uart_poll_dma_tx(ESP8266_UART_ID);
    
    /* 等待发送完成确认 */
    uint16_t resp_len = esp8266_read_response(esp8266_rx_buffer, ESP8266_RX_BUF_SIZE, 5000);
    if (resp_len == 0 || !strstr(esp8266_rx_buffer, "SEND OK")) {
        return ESP8266_ERROR;
    }
    
    return ESP8266_OK;
}

/**
 * @brief 接收TCP数据
 * @param buffer 接收缓冲区
 * @param max_len 缓冲区最大长度
 * @param timeout_ms 超时时间
 * @retval 实际接收到的字节数
 */
uint16_t esp8266_tcp_receive(uint8_t *buffer, uint16_t max_len, uint32_t timeout_ms)
{
    uint32_t start_time = HAL_GetTick();
    uint16_t total_received = 0;
    uint8_t temp_buffer[512];
    
    if (!tcp_connected || buffer == NULL) {
        return 0;
    }
    
    /* 等待接收数据 */
    while ((HAL_GetTick() - start_time) < timeout_ms && total_received < max_len) {
        /* 轮询串口发送 */
        uart_poll_dma_tx(ESP8266_UART_ID);
        
        /* 从串口读取数据到临时缓冲区 */
        uint16_t read_len = uart_read(ESP8266_UART_ID, temp_buffer, sizeof(temp_buffer));
        
        if (read_len > 0) {
            /* 查找+IPD包头 */
            uint16_t i = 0;
            while (i < read_len) {
                /* 检查是否为+IPD包头 */
                if (i + 4 < read_len && 
                    temp_buffer[i] == '+' && 
                    temp_buffer[i + 1] == 'I' &&
                    temp_buffer[i + 2] == 'P' &&
                    temp_buffer[i + 3] == 'D') {
                    
                    /* 查找逗号分隔符（+IPD,len:data格式） */
                    uint16_t comma_pos = i + 4;
                    while (comma_pos < read_len && temp_buffer[comma_pos] != ',') {
                        comma_pos++;
                    }
                    
                    if (comma_pos < read_len) {
                        /* 查找冒号分隔符 */
                        uint16_t colon_pos = comma_pos + 1;
                        while (colon_pos < read_len && temp_buffer[colon_pos] != ':') {
                            colon_pos++;
                        }
                        
                        if (colon_pos < read_len) {
                            /* 提取数据长度 */
                            char len_str[16];
                            uint16_t len_str_size = colon_pos - comma_pos - 1;
                            if (len_str_size < sizeof(len_str)) {
                                memcpy(len_str, &temp_buffer[comma_pos + 1], len_str_size);
                                len_str[len_str_size] = '\0';
                                uint16_t ipd_data_len = atoi(len_str);
                                
                                /* 复制实际数据到目标缓冲区 */
                                uint16_t data_start = colon_pos + 1;
                                uint16_t available_data = read_len - data_start;
                                uint16_t copy_len = (ipd_data_len < available_data) ? ipd_data_len : available_data;
                                copy_len = (copy_len < (max_len - total_received)) ? copy_len : (max_len - total_received);
                                
                                if (copy_len > 0) {
                                    memcpy(&buffer[total_received], &temp_buffer[data_start], copy_len);
                                    total_received += copy_len;
                                }
                                
                                /* 跳过已处理的数据 */
                                i = read_len; /* 跳出循环，处理下一次读取 */
                                break;
                            }
                        }
                    }
                    /* 没有找到完整的+IPD头，跳过这个字符 */
                    i++;
                } else {
                    /* 不是+IPD包头，可能是裸数据，直接复制 */
                    if (total_received < max_len) {
                        buffer[total_received++] = temp_buffer[i];
                    }
                    i++;
                }
            }
            
            /* 重置超时计时器，因为收到了数据 */
            start_time = HAL_GetTick();
        } else {
            /* 没有收到数据，短暂延时 */
            HAL_Delay(5);
        }
    }
    
    return total_received;
}

/**
 * @brief 关闭TCP连接
 * @retval ESP8266状态
 */
esp8266_status_t esp8266_tcp_close(void)
{
    esp8266_status_t status = esp8266_send_cmd("AT+CIPCLOSE", "OK", ESP8266_CMD_TIMEOUT_MS);
    tcp_connected = false;
    return status;
}

/* HTTP下载功能 */
static esp8266_http_info_t current_http_info = {0};
static uint16_t prefetch_data_len = 0;

/**
 * @brief 开始HTTP GET请求
 * @param url 完整的HTTP URL
 * @param info HTTP信息结构体指针
 * @retval ESP8266状态
 */
esp8266_status_t esp8266_http_get_start(const char *url, esp8266_http_info_t *info)
{
    char host[64];
    char path[128];
    uint16_t port = 80;
    char http_request[256];
    esp8266_status_t status;
    char *temp_ptr;
    
    if (url == NULL || info == NULL) {
        return ESP8266_ERROR;
    }
    
    /* 解析URL */
    if (strncmp(url, "http://", 7) == 0) {
        const char *url_start = url + 7; /* 跳过 "http://" */
        
        /* 提取主机名和路径 */
        temp_ptr = strchr(url_start, '/');
        if (temp_ptr != NULL) {
            /* 有路径 */
            uint16_t host_len = temp_ptr - url_start;
            strncpy(host, url_start, host_len);
            host[host_len] = '\0';
            strcpy(path, temp_ptr);
        } else {
            /* 没有路径，使用根路径 */
            strcpy(host, url_start);
            strcpy(path, "/");
        }
        
        /* 检查是否有端口号 */
        temp_ptr = strchr(host, ':');
        if (temp_ptr != NULL) {
            *temp_ptr = '\0';
            port = atoi(temp_ptr + 1);
        }
    } else {
        return ESP8266_ERROR; /* 不支持的URL格式 */
    }
    
    /* 建立TCP连接 */
    status = esp8266_tcp_connect(host, port);
    if (status != ESP8266_OK) {
        return status;
    }
    
    /* 构造HTTP GET请求 */
    snprintf(http_request, sizeof(http_request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "\r\n",
             path, host);
    
    /* 发送HTTP请求 */
    status = esp8266_tcp_send((uint8_t *)http_request, strlen(http_request));
    if (status != ESP8266_OK) {
        esp8266_tcp_close();
        return status;
    }
    
    /* 给服务器一点时间处理请求 */
    HAL_Delay(500);
    
    /* 读取HTTP响应头 */
    uint32_t start_time = HAL_GetTick();
    uint16_t response_len = 0;
    bool header_complete = false;
    char *header_end = NULL;
    char *full_response = esp8266_rx_buffer;
    
    memset(esp8266_rx_buffer, 0, ESP8266_RX_BUF_SIZE);
    
    while ((HAL_GetTick() - start_time) < 15000 && !header_complete) { /* 15秒超时 */
        uint16_t read_len = esp8266_tcp_receive((uint8_t *)&esp8266_rx_buffer[response_len], 
                                              ESP8266_RX_BUF_SIZE - response_len - 1, 200);
        if (read_len > 0) {
            response_len += read_len;
            esp8266_rx_buffer[response_len] = '\0';
            
            /* 检查是否收到完整HTTP头 */
            header_end = strstr(esp8266_rx_buffer, "\r\n\r\n");
            if (header_end != NULL) {
                header_complete = true;
            }
        } else {
            HAL_Delay(10);
        }
    }
    
    if (!header_complete) {
        esp8266_tcp_close();
        return ESP8266_TIMEOUT;
    }
    
    /* 现在分离HTTP头和body数据 */
    char *body_start = header_end + 4; /* 跳过 "\r\n\r\n" */
    uint16_t header_actual_len = header_end - esp8266_rx_buffer;
    uint16_t body_bytes_in_buffer = response_len - header_actual_len - 4;
    
    /* 保存预读取的body数据 */
    if (body_bytes_in_buffer > 0) {
        memmove(esp8266_rx_buffer, body_start, body_bytes_in_buffer);
        prefetch_data_len = body_bytes_in_buffer;
    } else {
        prefetch_data_len = 0;
    }
    
    /* 临时终止HTTP头以便解析 */
    *header_end = '\0';
    
    /* 解析HTTP状态码（现在HTTP头已被null终止） */
    if (strstr(full_response, "200 OK") == NULL) {
        esp8266_tcp_close();
        return ESP8266_ERROR; /* HTTP错误 */
    }
    
    /* 解析Content-Length（从HTTP头中解析） */
    temp_ptr = strstr(full_response, "Content-Length:");
    if (temp_ptr != NULL) {
        sscanf(temp_ptr, "Content-Length: %lu", &info->content_length);
        info->chunked = false;
    } else {
        /* 检查是否为分块传输 */
        temp_ptr = strstr(full_response, "Transfer-Encoding: chunked");
        if (temp_ptr != NULL) {
            info->chunked = true;
            info->content_length = 0; /* 分块传输无法预知长度 */
        } else {
            info->content_length = 0;
            info->chunked = false;
        }
    }
    
    info->downloaded = 0;
    memcpy(&current_http_info, info, sizeof(esp8266_http_info_t));
    
    /* 注意：prefetch_data_len 在解析HTTP头时已经设置 */
    
    return ESP8266_OK;
}

/**
 * @brief 获取HTTP数据
 * @param buffer 接收缓冲区
 * @param max_len 缓冲区最大长度
 * @param timeout_ms 超时时间
 * @retval 实际接收到的字节数，0表示传输结束
 */
uint16_t esp8266_http_get_data(uint8_t *buffer, uint16_t max_len, uint32_t timeout_ms)
{
    uint16_t received = 0;
    
    /* 先使用预取的数据 */
    if (prefetch_data_len > 0) {
        uint16_t copy_len = (prefetch_data_len < max_len) ? prefetch_data_len : max_len;
        memcpy(buffer, esp8266_rx_buffer, copy_len);
        received = copy_len;
        
        /* 更新预取数据 */
        if (copy_len < prefetch_data_len) {
            /* 还有剩余数据 */
            memmove(esp8266_rx_buffer, &esp8266_rx_buffer[copy_len], prefetch_data_len - copy_len);
            prefetch_data_len -= copy_len;
        } else {
            /* 预取数据用完 */
            prefetch_data_len = 0;
        }
    }
    
    /* 如果缓冲区还有空间且没有预取数据，继续从TCP读取 */
    if (received < max_len && prefetch_data_len == 0) {
        uint16_t tcp_received = esp8266_tcp_receive(&buffer[received], max_len - received, timeout_ms);
        received += tcp_received;
    }
    
    current_http_info.downloaded += received;
    return received;
}

/**
 * @brief 结束HTTP GET请求
 * @retval ESP8266状态
 */
esp8266_status_t esp8266_http_get_finish(void)
{
    return esp8266_tcp_close();
}

/**
 * @brief 测试TCP连接
 * @param host 主机名或IP
 * @param port 端口号
 * @retval ESP8266状态
 */
esp8266_status_t esp8266_test_connection(const char *host, uint16_t port)
{
    esp8266_status_t status;
    
    status = esp8266_tcp_connect(host, port);
    if (status == ESP8266_OK) {
        esp8266_tcp_close();
    }
    
    return status;
}