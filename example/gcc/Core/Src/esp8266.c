#include "esp8266.h"
#include "dev_usart.h"
#include "gpio.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ==================== 私有变量 ==================== */

/* 接收缓冲区 */
static uint8_t rx_buffer[ESP8266_RX_BUF_SIZE];
static uint16_t rx_index = 0;

/* WiFi连接信息 */
static wifi_config_t wifi_config = {0};
static wifi_status_t wifi_status = WIFI_DISCONNECTED;
static char wifi_ip[16] = {0};

/* TCP连接信息 */
static tcp_info_t tcp_info = {0};

/* HTTP响应信息 */
static http_response_t http_resp = {0};
static uint8_t http_buffer[1024];  /* HTTP数据缓冲区 */
static uint16_t http_data_len = 0; /* 缓冲区中的数据长度 */

/* OTA信息 */
static ota_info_t ota_info = {0};

/* ==================== 私有函数声明 ==================== */

static void esp_hardware_reset(void);
static void esp_clear_buffer(void);
static esp_result_t esp_wait_response(const char *expect, uint32_t timeout);
static esp_result_t esp_parse_ipd_data(uint8_t *buffer, uint16_t max_len, uint16_t *received);
static void print_str(const char* str);

/* ==================== 基础功能实现 ==================== */

/**
 * @brief 打印字符串（调试用）
 */
static void print_str(const char* str)
{
    if (str == NULL) return;
    uart_write(DEV_UART1, (uint8_t *)str, strlen(str));
    uart_poll_dma_tx(DEV_UART1);
}

/**
 * @brief 硬件复位ESP8266
 */
static void esp_hardware_reset(void)
{
    /* 拉低复位引脚 */
    HAL_GPIO_WritePin(ESP8266_RST_PORT, ESP8266_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(100);
    
    /* 拉高复位引脚 */
    HAL_GPIO_WritePin(ESP8266_RST_PORT, ESP8266_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(2000);  /* 等待启动 */
}

/**
 * @brief 清空接收缓冲区
 */
static void esp_clear_buffer(void)
{
    uint8_t temp[128];
    uint32_t start = HAL_GetTick();
    
    /* 清空串口缓冲区 */
    while ((HAL_GetTick() - start) < 100) {
        if (uart_read(ESP8266_UART_ID, temp, sizeof(temp)) == 0) {
            break;
        }
    }
    
    /* 清空内部缓冲区 */
    memset(rx_buffer, 0, sizeof(rx_buffer));
    rx_index = 0;
}

/**
 * @brief 等待特定响应
 * @param expect 期望的响应字符串
 * @param timeout 超时时间(ms)
 * @retval ESP结果
 */
static esp_result_t esp_wait_response(const char *expect, uint32_t timeout)
{
    uint32_t start = HAL_GetTick();
    uint16_t len = 0;
    
    while ((HAL_GetTick() - start) < timeout) {
        /* 轮询发送 */
        uart_poll_dma_tx(ESP8266_UART_ID);
        
        /* 读取数据 */
        len = uart_read(ESP8266_UART_ID, &rx_buffer[rx_index], 
                       ESP8266_RX_BUF_SIZE - rx_index - 1);
        if (len > 0) {
            rx_index += len;
            rx_buffer[rx_index] = '\0';
            
            /* 检查期望的响应 */
            if (strstr((char *)rx_buffer, expect) != NULL) {
                return ESP_OK;
            }
            
            /* 检查错误响应 */
            if (strstr((char *)rx_buffer, "ERROR") != NULL) {
                return ESP_ERROR;
            }
            
            /* 防止缓冲区溢出 */
            if (rx_index >= ESP8266_RX_BUF_SIZE - 100) {
                memmove(rx_buffer, &rx_buffer[100], rx_index - 100);
                rx_index -= 100;
            }
        }
        
        HAL_Delay(10);
    }
    
    return ESP_TIMEOUT;
}

/**
 * @brief 初始化ESP8266
 * @retval ESP结果
 */
esp_result_t esp_init(void)
{
    esp_result_t result;
    
    /* 初始化串口 */
    uart_device_init(ESP8266_UART_ID);
    
    /* 配置复位引脚 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOE_CLK_ENABLE();
    
    GPIO_InitStruct.Pin = ESP8266_RST_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(ESP8266_RST_PORT, &GPIO_InitStruct);
    
    /* 硬件复位 */
    esp_hardware_reset();
    
    /* 测试AT命令 */
    result = esp_test();
    if (result != ESP_OK) {
        return result;
    }
    
    /* 关闭回显 */
    result = esp_send_command("ATE0", "OK", ESP8266_CMD_TIMEOUT);
    if (result != ESP_OK) {
        return result;
    }
    
    /* 设置为Station模式 */
    result = esp_send_command("AT+CWMODE=1", "OK", ESP8266_CMD_TIMEOUT);
    if (result != ESP_OK) {
        return result;
    }
    
    /* 设置单连接模式 */
    result = esp_send_command("AT+CIPMUX=0", "OK", ESP8266_CMD_TIMEOUT);
    if (result != ESP_OK) {
        return result;
    }
    
    /* 设置传输模式为普通模式 */
    result = esp_send_command("AT+CIPMODE=0", "OK", ESP8266_CMD_TIMEOUT);
    
    return result;
}

/**
 * @brief 复位ESP8266
 * @retval ESP结果
 */
esp_result_t esp_reset(void)
{
    /* 硬件复位 */
    esp_hardware_reset();
    
    /* 等待启动完成 */
    HAL_Delay(2000);
    
    /* 测试通信 */
    return esp_test();
}

/**
 * @brief 测试ESP8266通信
 * @retval ESP结果
 */
esp_result_t esp_test(void)
{
    uint8_t retry = 3;
    
    while (retry--) {
        esp_clear_buffer();
        
        if (esp_send_command("AT", "OK", 1000) == ESP_OK) {
            return ESP_OK;
        }
        
        HAL_Delay(500);
    }
    
    return ESP_ERROR;
}

/**
 * @brief 获取版本信息
 * @param version 版本字符串缓冲区
 * @param max_len 缓冲区最大长度
 * @retval ESP结果
 */
esp_result_t esp_get_version(char *version, uint16_t max_len)
{
    esp_result_t result;
    
    esp_clear_buffer();
    
    result = esp_send_command("AT+GMR", "OK", ESP8266_CMD_TIMEOUT);
    if (result == ESP_OK && version != NULL) {
        strncpy(version, (char *)rx_buffer, max_len - 1);
        version[max_len - 1] = '\0';
    }
    
    return result;
}

/**
 * @brief 发送AT命令
 * @param cmd 命令字符串
 * @param expect 期望的响应
 * @param timeout 超时时间
 * @retval ESP结果
 */
esp_result_t esp_send_command(const char *cmd, const char *expect, uint32_t timeout)
{
    if (cmd == NULL) {
        return ESP_INVALID_PARAM;
    }
    
    /* 清空缓冲区 */
    esp_clear_buffer();
    
    /* 发送命令 */
    uart_write(ESP8266_UART_ID, (uint8_t *)cmd, strlen(cmd));
    uart_write(ESP8266_UART_ID, (uint8_t *)"\r\n", 2);
    uart_poll_dma_tx(ESP8266_UART_ID);
    
    /* 等待响应 */
    if (expect != NULL) {
        return esp_wait_response(expect, timeout);
    }
    
    return ESP_OK;
}

/**
 * @brief 发送数据
 * @param data 数据指针
 * @param len 数据长度
 * @retval ESP结果
 */
esp_result_t esp_send_data(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) {
        return ESP_INVALID_PARAM;
    }
    
    uart_write(ESP8266_UART_ID, data, len);
    uart_poll_dma_tx(ESP8266_UART_ID);
    
    return ESP_OK;
}

/**
 * @brief 接收数据
 * @param buffer 接收缓冲区
 * @param max_len 最大接收长度
 * @param timeout 超时时间
 * @retval 实际接收的字节数
 */
uint16_t esp_receive_data(uint8_t *buffer, uint16_t max_len, uint32_t timeout)
{
    uint32_t start = HAL_GetTick();
    uint16_t total = 0;
    
    while ((HAL_GetTick() - start) < timeout && total < max_len) {
        uint16_t len = uart_read(ESP8266_UART_ID, &buffer[total], max_len - total);
        if (len > 0) {
            total += len;
            start = HAL_GetTick();  /* 重置超时 */
        }
        HAL_Delay(10);
    }
    
    return total;
}

/* ==================== WiFi功能实现 ==================== */

/**
 * @brief 连接WiFi
 * @param ssid WiFi名称
 * @param password WiFi密码
 * @retval ESP结果
 */
esp_result_t esp_wifi_connect(const char *ssid, const char *password)
{
    char cmd[128];
    esp_result_t result;
    
    if (ssid == NULL) {
        return ESP_INVALID_PARAM;
    }
    
    /* 保存WiFi配置 */
    strncpy(wifi_config.ssid, ssid, sizeof(wifi_config.ssid) - 1);
    if (password != NULL) {
        strncpy(wifi_config.password, password, sizeof(wifi_config.password) - 1);
    }
    
    /* 断开当前连接 */
    esp_wifi_disconnect();
    
    /* 设置自动连接 */
    esp_send_command("AT+CWAUTOCONN=1", "OK", ESP8266_CMD_TIMEOUT);
    
    /* 连接WiFi */
    if (password != NULL && strlen(password) > 0) {
        snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    } else {
        snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"\"", ssid);
    }
    
    wifi_status = WIFI_CONNECTING;
    result = esp_send_command(cmd, "OK", ESP8266_WIFI_TIMEOUT);
    
    if (result == ESP_OK) {
        wifi_status = WIFI_GOT_IP;
        
        /* 获取IP地址 */
        esp_wifi_get_ip(wifi_ip, sizeof(wifi_ip));
        
        /* 禁用休眠模式 */
        esp_send_command("AT+SLEEP=0", "OK", ESP8266_CMD_TIMEOUT);
    } else {
        wifi_status = WIFI_DISCONNECTED;
    }
    
    return result;
}

/**
 * @brief 断开WiFi连接
 * @retval ESP结果
 */
esp_result_t esp_wifi_disconnect(void)
{
    esp_result_t result = esp_send_command("AT+CWQAP", "OK", ESP8266_CMD_TIMEOUT);
    wifi_status = WIFI_DISCONNECTED;
    memset(wifi_ip, 0, sizeof(wifi_ip));
    return result;
}

/**
 * @brief 重连WiFi
 * @retval ESP结果
 */
esp_result_t esp_wifi_reconnect(void)
{
    if (strlen(wifi_config.ssid) == 0) {
        return ESP_ERROR;
    }
    
    return esp_wifi_connect(wifi_config.ssid, 
                           strlen(wifi_config.password) > 0 ? wifi_config.password : NULL);
}

/**
 * @brief 获取WiFi状态
 * @retval WiFi状态
 */
wifi_status_t esp_wifi_get_status(void)
{
    return wifi_status;
}

/**
 * @brief 获取IP地址
 * @param ip IP地址缓冲区
 * @param max_len 缓冲区最大长度
 * @retval ESP结果
 */
esp_result_t esp_wifi_get_ip(char *ip, uint16_t max_len)
{
    esp_result_t result;
    char *ptr;
    
    esp_clear_buffer();
    
    result = esp_send_command("AT+CIFSR", "OK", ESP8266_CMD_TIMEOUT);
    if (result == ESP_OK && ip != NULL) {
        /* 查找STAIP */
        ptr = strstr((char *)rx_buffer, "STAIP,\"");
        if (ptr != NULL) {
            ptr += 7;  /* 跳过 STAIP," */
            char *end = strchr(ptr, '"');
            if (end != NULL) {
                uint16_t len = end - ptr;
                if (len < max_len) {
                    strncpy(ip, ptr, len);
                    ip[len] = '\0';
                    return ESP_OK;
                }
            }
        }
    }
    
    return ESP_ERROR;
}

/**
 * @brief 检查WiFi是否已连接
 * @retval true已连接，false未连接
 */
bool esp_wifi_is_connected(void)
{
    esp_clear_buffer();
    
    if (esp_send_command("AT+CWJAP?", "OK", ESP8266_CMD_TIMEOUT) == ESP_OK) {
        if (strstr((char *)rx_buffer, "+CWJAP:\"") != NULL) {
            wifi_status = WIFI_GOT_IP;
            return true;
        }
    }
    
    wifi_status = WIFI_DISCONNECTED;
    return false;
}

/* ==================== TCP功能实现 ==================== */

/**
 * @brief 建立TCP连接
 * @param host 主机地址
 * @param port 端口号
 * @retval ESP结果
 */
esp_result_t esp_tcp_connect(const char *host, uint16_t port)
{
    char cmd[128];
    esp_result_t result;
    
    if (host == NULL) {
        return ESP_INVALID_PARAM;
    }
    
    /* 检查WiFi连接 */
    if (!esp_wifi_is_connected()) {
        return ESP_NO_WIFI;
    }
    
    /* 关闭之前的连接 */
    if (tcp_info.status == TCP_CONNECTED) {
        esp_tcp_disconnect();
        HAL_Delay(500);
    }
    
    /* 保存连接信息 */
    strncpy(tcp_info.host, host, sizeof(tcp_info.host) - 1);
    tcp_info.port = port;
    tcp_info.status = TCP_CONNECTING;
    
    /* 建立TCP连接 */
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d", host, port);
    
    print_str("DEBUG: TCP Connect command: ");
    print_str(cmd);
    print_str("\r\n");
    
    esp_clear_buffer();
    uart_write(ESP8266_UART_ID, (uint8_t *)cmd, strlen(cmd));
    uart_write(ESP8266_UART_ID, (uint8_t *)"\r\n", 2);
    uart_poll_dma_tx(ESP8266_UART_ID);
    
    /* 等待连接响应 */
    result = esp_wait_response("CONNECT", ESP8266_TCP_TIMEOUT);
    
    if (result == ESP_OK) {
        tcp_info.status = TCP_CONNECTED;
        print_str("DEBUG: TCP Connected successfully\r\n");
    } else {
        tcp_info.status = TCP_DISCONNECTED;
        print_str("DEBUG: TCP Connection failed\r\n");
    }
    
    return result;
}

/**
 * @brief 断开TCP连接
 * @retval ESP结果
 */
esp_result_t esp_tcp_disconnect(void)
{
    esp_result_t result;
    
    if (tcp_info.status != TCP_CONNECTED) {
        return ESP_OK;
    }
    
    tcp_info.status = TCP_CLOSING;
    result = esp_send_command("AT+CIPCLOSE", "CLOSED", ESP8266_CMD_TIMEOUT);
    tcp_info.status = TCP_DISCONNECTED;
    
    return result;
}

/**
 * @brief 发送TCP数据
 * @param data 数据指针
 * @param len 数据长度
 * @retval ESP结果
 */
esp_result_t esp_tcp_send(const uint8_t *data, uint16_t len)
{
    char cmd[32];
    esp_result_t result;
    
    if (data == NULL || len == 0) {
        return ESP_INVALID_PARAM;
    }
    
    if (tcp_info.status != TCP_CONNECTED) {
        return ESP_NO_TCP;
    }
    
    /* 发送AT+CIPSEND命令 */
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", len);
    
    print_str("DEBUG: Sending data, length: ");
    char len_str[16];
    snprintf(len_str, sizeof(len_str), "%d\r\n", len);
    print_str(len_str);
    
    /* 不清空缓冲区，避免丢失可能已经到达的响应数据 */
    /* 直接发送命令，不调用esp_send_command以避免清空缓冲区 */
    uart_write(ESP8266_UART_ID, (uint8_t *)cmd, strlen(cmd));
    uart_write(ESP8266_UART_ID, (uint8_t *)"\r\n", 2);
    uart_poll_dma_tx(ESP8266_UART_ID);
    
    /* 等待">"提示符 */
    result = esp_wait_response(">", ESP8266_CMD_TIMEOUT);
    if (result != ESP_OK) {
        print_str("DEBUG: CIPSEND command failed\r\n");
        return result;
    }
    
    /* 发送实际数据 */
    uart_write(ESP8266_UART_ID, data, len);
    uart_poll_dma_tx(ESP8266_UART_ID);
    
    /* 等待发送完成 */
    result = esp_wait_response("SEND OK", ESP8266_CMD_TIMEOUT);
    
    if (result == ESP_OK) {
        print_str("DEBUG: Data sent successfully\r\n");
    } else {
        print_str("DEBUG: Data send failed\r\n");
    }
    
    return result;
}

/**
 * @brief 解析+IPD数据
 * @param buffer 接收缓冲区
 * @param max_len 最大接收长度
 * @param received 实际接收的字节数
 * @retval ESP结果
 */
static esp_result_t esp_parse_ipd_data(uint8_t *buffer, uint16_t max_len, uint16_t *received)
{
    char *ptr;
    char *data_start;
    uint16_t data_len = 0;
    uint16_t copy_len = 0;
    uint16_t total_parsed = 0;
    
    *received = 0;
    
    /* 查找+IPD头 */
    ptr = strstr((char *)rx_buffer, "+IPD,");
    if (ptr == NULL) {
        return ESP_ERROR;
    }
    
    /* 提取数据长度 */
    ptr += 5;  /* 跳过 "+IPD," */
    data_len = atoi(ptr);
    
    /* 调试信息简化 */
    
    /* 查找数据起始位置 */
    data_start = strchr(ptr, ':');
    if (data_start == NULL) {
        return ESP_ERROR;
    }
    data_start++;  /* 跳过 ':' */
    
    /* 计算可复制的数据长度 */
    copy_len = data_len;
    if (copy_len > max_len) {
        copy_len = max_len;
    }
    
    /* 检查缓冲区中是否有足够的数据 */
    uint16_t available = rx_index - (data_start - (char *)rx_buffer);
    if (copy_len > available) {
        copy_len = available;
    }
    
    /* 复制数据 */
    if (copy_len > 0) {
        memcpy(buffer, data_start, copy_len);
        *received = copy_len;
        total_parsed = (data_start - (char *)rx_buffer) + copy_len;
        
        /* 移除已处理的数据，保留剩余数据 */
        if (total_parsed < rx_index) {
            memmove(rx_buffer, &rx_buffer[total_parsed], rx_index - total_parsed);
            rx_index -= total_parsed;
        } else {
            rx_index = 0;
        }
        rx_buffer[rx_index] = '\0';
        
        /* 简化调试信息 */
    }
    
    return ESP_OK;
}

/**
 * @brief 接收TCP数据
 * @param buffer 接收缓冲区
 * @param max_len 最大接收长度
 * @param timeout 超时时间
 * @retval 实际接收的字节数
 */
uint16_t esp_tcp_receive(uint8_t *buffer, uint16_t max_len, uint32_t timeout)
{
    uint32_t start = HAL_GetTick();
    uint16_t total_received = 0;
    uint16_t received = 0;
    
    if (buffer == NULL || tcp_info.status != TCP_CONNECTED) {
        return 0;
    }
    
    /* 检查是否有遗留的数据 */
    if (rx_index > 0) {
        print_str("DEBUG: Found data in buffer: ");
        char tmsg[32];
        snprintf(tmsg, sizeof(tmsg), "%d bytes\r\n", rx_index);
        print_str(tmsg);
        
        /* 尝试解析所有已有的+IPD数据 */
        while (strstr((char *)rx_buffer, "+IPD,") != NULL && total_received < max_len) {
            if (esp_parse_ipd_data(&buffer[total_received], 
                                  max_len - total_received, &received) == ESP_OK) {
                if (received > 0) {
                    total_received += received;
                } else {
                    break;  /* 没有更多完整的数据 */
                }
            } else {
                break;  /* 解析失败，可能数据不完整 */
            }
        }
        
        /* 如果解析到数据，立即返回 */
        if (total_received > 0) {
            return total_received;
        }
    }
    
    /* 去掉调试信息，减少干扰 */
    
    while ((HAL_GetTick() - start) < timeout && total_received < max_len) {
        /* 轮询发送 */
        uart_poll_dma_tx(ESP8266_UART_ID);
        
        /* 读取数据到内部缓冲区 */
        uint16_t len = uart_read(ESP8266_UART_ID, &rx_buffer[rx_index], 
                                ESP8266_RX_BUF_SIZE - rx_index - 1);
        if (len > 0) {
            rx_index += len;
            rx_buffer[rx_index] = '\0';
            
            /* 检查是否有+IPD数据 */
            char *ipd_ptr = strstr((char *)rx_buffer, "+IPD,");
            if (ipd_ptr != NULL) {
                /* 尝试解析+IPD数据 */
                /* 循环解析所有+IPD数据包 */
                while (strstr((char *)rx_buffer, "+IPD,") != NULL && total_received < max_len) {
                    if (esp_parse_ipd_data(&buffer[total_received], 
                                          max_len - total_received, &received) == ESP_OK) {
                        if (received > 0) {
                            total_received += received;
                            start = HAL_GetTick();  /* 重置超时 */
                        } else {
                            break;  /* 没有更多数据 */
                        }
                    } else {
                        break;  /* 解析失败，等待更多数据 */
                    }
                }
            }
            
            /* 检查连接是否关闭 */
            if (strstr((char *)rx_buffer, "CLOSED") != NULL) {
                tcp_info.status = TCP_DISCONNECTED;
                print_str("DEBUG: TCP connection closed\r\n");
                break;
            }
            
            /* 防止缓冲区溢出 */
            if (rx_index >= ESP8266_RX_BUF_SIZE - 100) {
                /* 缓冲区快满了，尝试解析所有可用数据 */
                while (strstr((char *)rx_buffer, "+IPD,") != NULL && total_received < max_len) {
                    if (esp_parse_ipd_data(&buffer[total_received], 
                                          max_len - total_received, &received) == ESP_OK) {
                        if (received > 0) {
                            total_received += received;
                            start = HAL_GetTick();
                        } else {
                            break;
                        }
                    } else {
                        /* 解析失败，可能数据不完整，等待更多数据 */
                        break;
                    }
                }
                
                /* 如果解析了一些数据，返回 */
                if (total_received > 0) {
                    return total_received;
                }
                
                /* 如果缓冲区仍然太满且没有解析到数据，清理一部分 */
                if (rx_index >= ESP8266_RX_BUF_SIZE - 100) {
                    /* 查找最后一个+IPD的位置 */
                    char *last_ipd = strstr((char *)rx_buffer, "+IPD,");
                    if (last_ipd != NULL) {
                        /* 保留最后一个+IPD及其后的数据 */
                        uint32_t keep_from = last_ipd - (char *)rx_buffer;
                        memmove(rx_buffer, &rx_buffer[keep_from], rx_index - keep_from);
                        rx_index = rx_index - keep_from;
                    } else {
                        /* 没有+IPD，清空一半 */
                        memmove(rx_buffer, &rx_buffer[rx_index/2], rx_index - rx_index/2);
                        rx_index = rx_index - rx_index/2;
                    }
                }
            }
        }
        
        HAL_Delay(10);
    }
    
    return total_received;
}

/**
 * @brief 获取TCP连接状态
 * @retval TCP状态
 */
tcp_status_t esp_tcp_get_status(void)
{
    return tcp_info.status;
}

/**
 * @brief 检查TCP是否已连接
 * @retval true已连接，false未连接
 */
bool esp_tcp_is_connected(void)
{
    return (tcp_info.status == TCP_CONNECTED);
}

/* ==================== HTTP功能实现 ==================== */

/**
 * @brief 发起HTTP GET请求
 * @param url 完整的URL
 * @param response HTTP响应信息
 * @retval ESP结果
 */
esp_result_t esp_http_get(const char *url, http_response_t *response)
{
    char host[64] = {0};
    char path[128] = {0};
    uint16_t port = 80;
    char request[256];
    esp_result_t result;
    char *ptr;
    
    if (url == NULL || response == NULL) {
        return ESP_INVALID_PARAM;
    }
    
    /* 解析URL */
    if (strncmp(url, "http://", 7) == 0) {
        url += 7;  /* 跳过 "http://" */
    } else {
        return ESP_INVALID_PARAM;
    }
    
    /* 提取主机名 */
    ptr = strchr(url, '/');
    if (ptr != NULL) {
        size_t host_len = ptr - url;
        if (host_len >= sizeof(host)) {
            return ESP_INVALID_PARAM;
        }
        strncpy(host, url, host_len);
        strncpy(path, ptr, sizeof(path) - 1);
    } else {
        strncpy(host, url, sizeof(host) - 1);
        strcpy(path, "/");
    }
    
    /* 提取端口号 */
    ptr = strchr(host, ':');
    if (ptr != NULL) {
        *ptr = '\0';
        port = atoi(ptr + 1);
    }
    
    /* 建立TCP连接 */
    result = esp_tcp_connect(host, port);
    if (result != ESP_OK) {
        return result;
    }
    
    /* 构造HTTP请求 */
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: keep-alive\r\n"
             "\r\n", path, host);
    
    /* 在发送HTTP请求前清空缓冲区 */
    esp_clear_buffer();
    
    /* 发送HTTP请求 */
    result = esp_tcp_send((uint8_t *)request, strlen(request));
    if (result != ESP_OK) {
        esp_tcp_disconnect();
        return result;
    }
    
    /* 接收HTTP响应头 */
    print_str("DEBUG: Waiting for HTTP response...\r\n");
    
    /* 不清空缓冲区，直接开始接收 */
    /* esp_clear_buffer(); */
    uint32_t start = HAL_GetTick();
    bool header_complete = false;
    bool got_ipd = false;
    char *ipd_data_start = NULL;
    uint16_t ipd_data_len = 0;
    
    /* 首先等待+IPD头 */
    print_str("DEBUG: Waiting for +IPD...\r\n");
    while ((HAL_GetTick() - start) < 10000) {  /* 增加超时到10秒 */
        uint16_t len = uart_read(ESP8266_UART_ID, &rx_buffer[rx_index], 
                                ESP8266_RX_BUF_SIZE - rx_index - 1);
        if (len > 0) {
            /* 调试：显示接收到的字节数 */
            char dbg_msg[32];
            snprintf(dbg_msg, sizeof(dbg_msg), "DEBUG: Received %d bytes\r\n", len);
            print_str(dbg_msg);
            rx_index += len;
            rx_buffer[rx_index] = '\0';
            
            /* 查找+IPD头 */
            if (!got_ipd && strstr((char *)rx_buffer, "+IPD,") != NULL) {
                got_ipd = true;
                
                /* 解析+IPD长度 */
                char *ipd_ptr = strstr((char *)rx_buffer, "+IPD,");
                if (ipd_ptr != NULL) {
                    ipd_ptr += 5;  /* 跳过"+IPD," */
                    ipd_data_len = atoi(ipd_ptr);
                    
                    /* 找到数据起始位置 */
                    ipd_data_start = strchr(ipd_ptr, ':');
                    if (ipd_data_start != NULL) {
                        ipd_data_start++;  /* 跳过':' */
                        
                        /* 检查HTTP头是否完整 */
                        if (strstr(ipd_data_start, "\r\n\r\n") != NULL) {
                            header_complete = true;
                            break;
                        }
                    }
                }
            }
            
            /* 如果已经获取+IPD，继续检查HTTP头是否完整 */
            if (got_ipd && !header_complete && ipd_data_start != NULL) {
                if (strstr(ipd_data_start, "\r\n\r\n") != NULL) {
                    header_complete = true;
                    break;
                }
            }
            
            /* 防止缓冲区溢出 */
            if (rx_index >= ESP8266_RX_BUF_SIZE - 100) {
                break;
            }
        }
        HAL_Delay(10);
    }
    
    if (!got_ipd) {
        /* 调试：打印接收到的数据 */
        print_str("DEBUG: No +IPD header received. Buffer content:\r\n");
        print_str((char *)rx_buffer);
        print_str("\r\n");
        esp_tcp_disconnect();
        return ESP_TIMEOUT;
    }
    
    if (!header_complete) {
        /* 尝试查找更宽松的HTTP响应 */
        if (strstr((char *)rx_buffer, "HTTP/1.") != NULL) {
            /* 找到HTTP响应，即使没有完整的头也继续处理 */
            header_complete = true;
            print_str("DEBUG: Found HTTP response without complete header\r\n");
        } else {
            print_str("DEBUG: No HTTP header found. Buffer content:\r\n");
            print_str((char *)rx_buffer);
            print_str("\r\n");
            esp_tcp_disconnect();
            return ESP_TIMEOUT;
        }
    }
    
    /* 使用ipd_data_start作为HTTP数据的起始位置 */
    if (ipd_data_start != NULL) {
        /* 解析HTTP状态码 */
        ptr = strstr(ipd_data_start, "HTTP/1.");
        if (ptr != NULL) {
            ptr = strchr(ptr, ' ');
            if (ptr != NULL) {
                response->status_code = atoi(ptr + 1);
            }
        }
        
        /* 解析Content-Length */
        ptr = strstr(ipd_data_start, "Content-Length:");
        if (ptr != NULL) {
            ptr += 15;  /* 跳过 "Content-Length:" */
            while (*ptr == ' ') ptr++;  /* 跳过空格 */
            response->content_length = atoi(ptr);
            response->chunked = false;
        } else {
            /* 检查是否为分块传输 */
            if (strstr(ipd_data_start, "Transfer-Encoding: chunked") != NULL) {
                response->chunked = true;
                response->content_length = 0;
            }
        }
        
        /* 保存HTTP响应体数据 */
        ptr = strstr(ipd_data_start, "\r\n\r\n");
        if (ptr != NULL) {
            ptr += 4;  /* 跳过 "\r\n\r\n" */
            
            /* 计算HTTP体数据长度 */
            uint16_t body_offset = ptr - ipd_data_start;
            if (body_offset < ipd_data_len) {
                http_data_len = ipd_data_len - body_offset;
                if (http_data_len > 0 && http_data_len < sizeof(http_buffer)) {
                    memcpy(http_buffer, ptr, http_data_len);
                    print_str("DEBUG: Cached HTTP body data: ");
                    char dbg_msg[32];
                    snprintf(dbg_msg, sizeof(dbg_msg), "%d bytes\r\n", http_data_len);
                    print_str(dbg_msg);
                }
            }
        }
    } else {
        /* 如果没有找到+IPD，尝试从原始缓冲区解析 */
        ptr = strstr((char *)rx_buffer, "HTTP/1.");
        if (ptr != NULL) {
            ptr = strchr(ptr, ' ');
            if (ptr != NULL) {
                response->status_code = atoi(ptr + 1);
            }
        }
    }
    
    response->received = 0;
    memcpy(&http_resp, response, sizeof(http_response_t));
    
    return ESP_OK;
}

/**
 * @brief 读取HTTP数据
 * @param buffer 接收缓冲区
 * @param max_len 最大接收长度
 * @retval 实际接收的字节数
 */
uint16_t esp_http_read_data(uint8_t *buffer, uint16_t max_len)
{
    uint16_t copied = 0;
    
    if (buffer == NULL) {
        return 0;
    }
    
    /* 先返回缓存的数据 */
    if (http_data_len > 0) {
        copied = http_data_len;
        if (copied > max_len) {
            copied = max_len;
        }
        memcpy(buffer, http_buffer, copied);
        
        /* 移动剩余数据 */
        if (copied < http_data_len) {
            memmove(http_buffer, &http_buffer[copied], http_data_len - copied);
            http_data_len -= copied;
        } else {
            http_data_len = 0;
        }
        
        http_resp.received += copied;
        return copied;
    }
    
    /* 从TCP接收新数据 */
    copied = esp_tcp_receive(buffer, max_len, 1000);
    http_resp.received += copied;
    
    return copied;
}

/**
 * @brief 关闭HTTP连接
 * @retval ESP结果
 */
esp_result_t esp_http_close(void)
{
    http_data_len = 0;
    memset(&http_resp, 0, sizeof(http_resp));
    return esp_tcp_disconnect();
}

/* ==================== OTA功能实现 ==================== */

/**
 * @brief 开始OTA下载
 * @param url 固件URL
 * @param info OTA信息
 * @retval ESP结果
 */
esp_result_t esp_ota_start(const char *url, ota_info_t *info)
{
    esp_result_t result;
    http_response_t response;
    
    if (url == NULL || info == NULL) {
        return ESP_INVALID_PARAM;
    }
    
    /* 保存URL */
    strncpy(ota_info.url, url, sizeof(ota_info.url) - 1);
    
    /* 发起HTTP请求 */
    result = esp_http_get(url, &response);
    if (result != ESP_OK) {
        return result;
    }
    
    /* 检查HTTP状态码 */
    if (response.status_code != 200) {
        esp_http_close();
        return ESP_ERROR;
    }
    
    /* 保存OTA信息 */
    ota_info.total_size = response.content_length;
    ota_info.downloaded = 0;
    ota_info.progress = 0;
    
    memcpy(info, &ota_info, sizeof(ota_info_t));
    
    return ESP_OK;
}

/**
 * @brief 读取OTA数据
 * @param buffer 接收缓冲区
 * @param max_len 最大接收长度
 * @retval 实际接收的字节数
 */
uint16_t esp_ota_read(uint8_t *buffer, uint16_t max_len)
{
    uint16_t received = 0;
    uint16_t total_received = 0;
    uint32_t start_time = HAL_GetTick();
    
    /* 类似RT-Thread的连续读取模式 */
    do {
        /* 先尝试从缓存读取 */
        received = esp_http_read_data(&buffer[total_received], max_len - total_received);
        
        if (received > 0) {
            total_received += received;
            start_time = HAL_GetTick();  /* 重置超时 */
        }
        
        /* 如果缓存为空且未达到最大长度，继续从 TCP 读取 */
        if (received == 0 && total_received < max_len) {
            received = esp_tcp_receive(&buffer[total_received], max_len - total_received, 1000);
            if (received > 0) {
                total_received += received;
                start_time = HAL_GetTick();
            }
        }
        
        /* 检查超时 */
        if ((HAL_GetTick() - start_time) > 10000) {
            break;  /* 10秒超时 */
        }
        
        /* 如果数据为0且连接关闭，退出 */
        if (received == 0 && !esp_tcp_is_connected()) {
            break;
        }
        
        HAL_Delay(10);  /* 稍作等待 */
        
    } while (total_received < max_len && received >= 0);
    
    if (total_received > 0) {
        ota_info.downloaded += total_received;
        
        /* 计算进度 */
        if (ota_info.total_size > 0) {
            ota_info.progress = (uint8_t)((ota_info.downloaded * 100) / ota_info.total_size);
        }
    }
    
    return total_received;
}

/**
 * @brief 结束OTA下载
 * @retval ESP结果
 */
esp_result_t esp_ota_finish(void)
{
    memset(&ota_info, 0, sizeof(ota_info));
    return esp_http_close();
}

/**
 * @brief 获取OTA下载进度
 * @retval 下载进度(0-100)
 */
uint8_t esp_ota_get_progress(void)
{
    return ota_info.progress;
}