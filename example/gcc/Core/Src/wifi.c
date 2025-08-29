#include "wifi.h"
#include "dev_usart.h"
#include "gpio.h"
#include "bootloader_cmd.h"  // 用于调试输出
#include <string.h>
#include <stdio.h>

#define WIFI_RST_PORT   GPIOE
#define WIFI_RST_PIN    GPIO_PIN_9
#define WIFI_UART       DEV_UART2

static wifi_state_t wifi_state = WIFI_STATE_IDLE;
static uint8_t response_buffer[512];

// 发送AT命令并等待响应
static bool wifi_send_cmd(const char *cmd, const char *expected, uint32_t timeout)
{
    uint8_t cmd_buf[128];
    int cmd_len = snprintf((char*)cmd_buf, sizeof(cmd_buf), "%s\r\n", cmd);
    
    // 清空接收缓冲区
    uint8_t temp;
    while(uart_read(WIFI_UART, &temp, 1) > 0);
    
    // 发送命令
    uart_write(WIFI_UART, cmd_buf, cmd_len);
    uart_poll_dma_tx(WIFI_UART);
    
    // 等待响应
    uint32_t start = HAL_GetTick();
    uint32_t recv_len = 0;
    
    while(HAL_GetTick() - start < timeout) {
        int len = uart_read(WIFI_UART, response_buffer + recv_len, 
                           sizeof(response_buffer) - recv_len - 1);
        if(len > 0) {
            recv_len += len;
            response_buffer[recv_len] = '\0';
            
            if(expected && strstr((char*)response_buffer, expected)) {
                return true;
            }
        }
        HAL_Delay(10);
    }
    
    return false;
}

// 硬件复位WiFi模块
static void wifi_hw_reset(void)
{
    HAL_GPIO_WritePin(WIFI_RST_PORT, WIFI_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(WIFI_RST_PORT, WIFI_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(500);
}

bool wifi_init(void)
{
    // 配置复位引脚
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = WIFI_RST_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(WIFI_RST_PORT, &GPIO_InitStruct);
    
    // 硬件复位
    wifi_hw_reset();
    
    // 多次尝试AT命令
    for(int i = 0; i < 3; i++) {
        if(wifi_send_cmd("AT", "OK", 2000)) {
            break;
        }
        if(i == 2) {
            wifi_state = WIFI_STATE_ERROR;
            return false;
        }
        HAL_Delay(500);
    }
    
    // 关闭回显
    wifi_send_cmd("ATE0", "OK", 1000);
    
    // 设置WiFi模式为Station
    if(!wifi_send_cmd("AT+CWMODE=1", "OK", 2000)) {
        wifi_state = WIFI_STATE_ERROR;
        return false;
    }
    
    // 设置单连接模式
    if(!wifi_send_cmd("AT+CIPMUX=0", "OK", 2000)) {
        wifi_state = WIFI_STATE_ERROR;
        return false;
    }
    
    wifi_state = WIFI_STATE_READY;
    return true;
}

bool wifi_connect(const char *ssid, const char *password)
{
    char cmd[128];
    
    if(wifi_state == WIFI_STATE_ERROR) {
        return false;
    }
    
    // 断开现有连接
    wifi_send_cmd("AT+CWQAP", "OK", 2000);
    
    // 连接WiFi
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    if(!wifi_send_cmd(cmd, "WIFI CONNECTED", WIFI_CONNECT_TIMEOUT)) {
        // 也检查GOT IP响应
        if(!strstr((char*)response_buffer, "WIFI GOT IP")) {
            wifi_state = WIFI_STATE_DISCONNECTED;
            return false;
        }
    }
    
    // 等待获取IP
    HAL_Delay(1000);
    
    wifi_state = WIFI_STATE_CONNECTED;
    return true;
}

bool wifi_disconnect(void)
{
    if(wifi_send_cmd("AT+CWQAP", "OK", 2000)) {
        wifi_state = WIFI_STATE_DISCONNECTED;
        return true;
    }
    return false;
}

bool wifi_is_connected(void)
{
    return wifi_state == WIFI_STATE_CONNECTED;
}

wifi_state_t wifi_get_state(void)
{
    return wifi_state;
}

bool wifi_reset(void)
{
    wifi_hw_reset();
    return wifi_init();
}

bool wifi_tcp_connect(const char *ip, uint16_t port)
{
    char cmd[128];
    
    if(wifi_state != WIFI_STATE_CONNECTED) {
        bootloader_print("WiFi: Not in connected state\r\n");
        return false;
    }
    
    // 先关闭可能存在的连接
    bootloader_print("WiFi: Closing existing connection...\r\n");
    wifi_send_cmd("AT+CIPCLOSE", "OK", 1000);
    HAL_Delay(500);
    
    // 建立TCP连接
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d", ip, port);
    bootloader_print("WiFi: Connecting with command: ");
    bootloader_print(cmd);
    bootloader_print("\r\n");
    
    // 清空缓冲区
    memset(response_buffer, 0, sizeof(response_buffer));
    
    // 发送连接命令
    uart_write(WIFI_UART, (uint8_t*)cmd, strlen(cmd));
    uart_write(WIFI_UART, (uint8_t*)"\r\n", 2);
    uart_poll_dma_tx(WIFI_UART);
    
    // 等待响应
    uint32_t start = HAL_GetTick();
    uint32_t recv_len = 0;
    bool connected = false;
    
    while(HAL_GetTick() - start < 10000) {
        int len = uart_read(WIFI_UART, response_buffer + recv_len, 
                           sizeof(response_buffer) - recv_len - 1);
        if(len > 0) {
            recv_len += len;
            response_buffer[recv_len] = '\0';
            
            if(strstr((char*)response_buffer, "CONNECT")) {
                bootloader_print("WiFi: TCP connected\r\n");
                connected = true;
                break;
            }
            if(strstr((char*)response_buffer, "ERROR") ||
               strstr((char*)response_buffer, "CLOSED")) {
                bootloader_print("WiFi: Connection failed. Response: ");
                bootloader_print((char*)response_buffer);
                bootloader_print("\r\n");
                return false;
            }
        }
        HAL_Delay(10);
    }
    
    if(!connected) {
        bootloader_print("WiFi: Connection timeout. Response: ");
        bootloader_print((char*)response_buffer);
        bootloader_print("\r\n");
        return false;
    }
    
    HAL_Delay(100);  // 等待连接稳定
    return true;
}

bool wifi_tcp_disconnect(void)
{
    return wifi_send_cmd("AT+CIPCLOSE", "CLOSED", 2000);
}

int32_t wifi_tcp_send(const uint8_t *data, uint32_t len)
{
    char cmd[32];
    uint32_t original_len = len;  // 保存原始长度
    
    // 检查连接状态 - 先发送AT测试
    bootloader_print("WiFi: Checking connection...\r\n");
    if(!wifi_send_cmd("AT+CIPSTATUS", "STATUS:", 1000)) {
        bootloader_print("WiFi: Connection check failed\r\n");
        return -1;
    }
    
    // 检查是否显示连接状态为3（已连接）
    if(!strstr((char*)response_buffer, "STATUS:3")) {
        bootloader_print("WiFi: Not connected (STATUS not 3)\r\n");
        bootloader_print("Response: ");
        bootloader_print((char*)response_buffer);
        bootloader_print("\r\n");
        return -1;
    }
    
    // 清空接收缓冲区
    uint8_t temp;
    while(uart_read(WIFI_UART, &temp, 1) > 0);
    
    // 发送CIPSEND命令
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%lu", (unsigned long)len);
    
    // 发送命令
    bootloader_print("WiFi: Sending CIPSEND command: ");
    bootloader_print(cmd);
    bootloader_print("\r\n");
    
    uart_write(WIFI_UART, (uint8_t*)cmd, strlen(cmd));
    uart_write(WIFI_UART, (uint8_t*)"\r\n", 2);
    uart_poll_dma_tx(WIFI_UART);
    
    // 等待 ">" 提示符
    uint32_t start = HAL_GetTick();
    uint32_t recv_len = 0;
    
    while(HAL_GetTick() - start < 3000) {
        int read_len = uart_read(WIFI_UART, response_buffer + recv_len, 
                                sizeof(response_buffer) - recv_len - 1);
        if(read_len > 0) {
            recv_len += read_len;
            response_buffer[recv_len] = '\0';
            
            if(strchr((char*)response_buffer, '>')) {
                bootloader_print("WiFi: Got '>' prompt\r\n");
                break;  // 收到提示符
            }
        }
        HAL_Delay(10);
    }
    
    if(!strchr((char*)response_buffer, '>')) {
        bootloader_print("WiFi: No '>' prompt received. Response: ");
        bootloader_print((char*)response_buffer);
        bootloader_print("\r\n");
        return -1;  // 没有收到提示符
    }
    
    // 发送数据
    uart_write(WIFI_UART, data, len);
    uart_poll_dma_tx(WIFI_UART);
    
    // 等待发送完成
    start = HAL_GetTick();
    recv_len = 0;
    memset(response_buffer, 0, sizeof(response_buffer));
    
    while(HAL_GetTick() - start < 5000) {
        int read_len = uart_read(WIFI_UART, response_buffer + recv_len, 
                                sizeof(response_buffer) - recv_len - 1);
        if(read_len > 0) {
            recv_len += read_len;
            response_buffer[recv_len] = '\0';
            
            if(strstr((char*)response_buffer, "SEND OK")) {
                return (int32_t)original_len;  // 返回原始数据长度
            }
            if(strstr((char*)response_buffer, "SEND FAIL") ||
               strstr((char*)response_buffer, "ERROR")) {
                return -1;
            }
        }
        HAL_Delay(10);
    }
    
    return -1;  // 超时
}

int32_t wifi_tcp_recv(uint8_t *buffer, uint32_t max_len, uint32_t timeout)
{
    uint32_t start = HAL_GetTick();
    uint32_t recv_len = 0;
    bool found_ipd = false;
    int expected_data_len = 0;
    int data_offset = 0;
    
    while(HAL_GetTick() - start < timeout) {
        int len = uart_read(WIFI_UART, buffer + recv_len, max_len - recv_len);
        if(len > 0) {
            recv_len += len;
            buffer[recv_len] = '\0';  // 确保字符串结束
            
            // 如果还没找到IPD头，继续寻找
            if(!found_ipd) {
                char *ipd = strstr((char*)buffer, "+IPD,");
                if(ipd) {
                    // 解析数据长度
                    if(sscanf(ipd, "+IPD,%d:", &expected_data_len) == 1) {
                        // 找到数据起始位置
                        char *data_start = strchr(ipd, ':');
                        if(data_start) {
                            data_start++;
                            data_offset = data_start - (char*)buffer;
                            found_ipd = true;
                            
                            bootloader_print("WiFi: Found IPD header, expecting ");
                            bootloader_print_dec(expected_data_len);
                            bootloader_print(" bytes of data\r\n");
                        }
                    }
                }
            }
            
            // 如果已找到IPD头，检查是否接收了足够的数据
            if(found_ipd) {
                int actual_data_len = recv_len - data_offset;
                if(actual_data_len >= expected_data_len) {
                    // 移动数据到缓冲区开头
                    memmove(buffer, buffer + data_offset, expected_data_len);
                    bootloader_print("WiFi: Received complete data (");
                    bootloader_print_dec(expected_data_len);
                    bootloader_print(" bytes)\r\n");
                    return expected_data_len;
                }
            }
        } else {
            // 没有新数据，稍作等待
            HAL_Delay(10);
        }
        
        // 防止缓冲区溢出
        if(recv_len >= max_len - 1) {
            break;
        }
    }
    
    // 超时或其他情况
    if(found_ipd && recv_len > data_offset) {
        int actual_data_len = recv_len - data_offset;
        memmove(buffer, buffer + data_offset, actual_data_len);
        bootloader_print("WiFi: Timeout, returning partial data (");
        bootloader_print_dec(actual_data_len);
        bootloader_print(" bytes)\r\n");
        return actual_data_len;
    }
    
    bootloader_print("WiFi: No valid data received\r\n");
    return -1;
}