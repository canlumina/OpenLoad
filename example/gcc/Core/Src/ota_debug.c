#include "bootloader_cmd.h"
#include "esp8266.h"
#include "dev_usart.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* 私有函数 */
static void print_str(const char* str)
{
    uart_write(DEV_UART1, (uint8_t*)str, strlen(str));
    uart_poll_dma_tx(DEV_UART1);
}

static void print_hex_byte(uint8_t byte)
{
    char buf[4];
    snprintf(buf, sizeof(buf), "%02X ", byte);
    print_str(buf);
}

static void print_dec(uint32_t val)
{
    char buf[12];
    snprintf(buf, sizeof(buf), "%lu", val);
    print_str(buf);
}

/**
 * @brief 调试HTTP连接
 */
void cmd_ota_debug_handler(void)
{
    esp_result_t result;
    char cmd[256];
    uint8_t buffer[512];
    uint16_t received;
    uint32_t start;
    
    print_str("\r\n=== OTA Debug Mode ===\r\n");
    
    /* 检查WiFi连接 */
    if(!esp_wifi_is_connected()) {
        print_str("WiFi not connected!\r\n");
        return;
    }
    
    print_str("WiFi connected, IP obtained.\r\n");
    
    /* 测试TCP连接到服务器 */
    print_str("\r\n1. Testing TCP connection...\r\n");
    print_str("Connecting to 115.190.137.231:3685...\r\n");
    
    /* 手动发送AT命令建立连接 */
    esp_send_command("AT+CIPCLOSE", NULL, 1000);  /* 先关闭之前的连接 */
    HAL_Delay(500);
    
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"115.190.137.231\",3685");
    print_str("Sending: ");
    print_str(cmd);
    print_str("\r\n");
    
    result = esp_send_command(cmd, "CONNECT", 10000);
    if(result != ESP_OK) {
        print_str("TCP connection failed!\r\n");
        if(result == ESP_TIMEOUT) {
            print_str("Timeout waiting for CONNECT\r\n");
        }
        return;
    }
    
    print_str("TCP connected successfully!\r\n");
    HAL_Delay(500);
    
    /* 发送HTTP请求 */
    print_str("\r\n2. Sending HTTP request...\r\n");
    const char *http_request = 
        "GET /api/firmware/download/latest HTTP/1.1\r\n"
        "Host: 115.190.137.231:3685\r\n"
        "User-Agent: STM32-OTA/1.0\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n"
        "\r\n";
    
    uint16_t req_len = strlen(http_request);
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", req_len);
    
    print_str("Sending CIPSEND: ");
    print_str(cmd);
    print_str("\r\n");
    
    result = esp_send_command(cmd, ">", 5000);
    if(result != ESP_OK) {
        print_str("CIPSEND failed!\r\n");
        esp_send_command("AT+CIPCLOSE", NULL, 1000);
        return;
    }
    
    print_str("Got prompt, sending HTTP data...\r\n");
    
    /* 发送HTTP数据 */
    uart_write(ESP8266_UART_ID, (uint8_t*)http_request, req_len);
    uart_poll_dma_tx(ESP8266_UART_ID);
    
    /* 等待SEND OK */
    HAL_Delay(1000);
    
    print_str("\r\n3. Reading response...\r\n");
    
    /* 读取响应（原始数据） */
    memset(buffer, 0, sizeof(buffer));
    start = HAL_GetTick();
    uint16_t total = 0;
    
    while((HAL_GetTick() - start) < 10000) {  /* 10秒超时 */
        received = uart_read(ESP8266_UART_ID, &buffer[total], sizeof(buffer) - total - 1);
        if(received > 0) {
            total += received;
            buffer[total] = '\0';
            
            /* 打印接收到的数据 */
            print_str("Received ");
            print_dec(received);
            print_str(" bytes\r\n");
            
            /* 检查是否收到+IPD */
            if(strstr((char*)buffer, "+IPD,") != NULL) {
                print_str("Got +IPD response!\r\n");
                break;
            }
            
            /* 检查是否收到CLOSED */
            if(strstr((char*)buffer, "CLOSED") != NULL) {
                print_str("Connection closed by server\r\n");
                break;
            }
        }
        HAL_Delay(100);
    }
    
    print_str("\r\n4. Response data (first 500 bytes):\r\n");
    print_str("--------------------\r\n");
    
    /* 打印前500字节的原始响应 */
    uint16_t print_len = (total < 500) ? total : 500;
    for(uint16_t i = 0; i < print_len; i++) {
        if(buffer[i] >= 32 && buffer[i] <= 126) {
            /* 可打印字符 */
            uart_write(DEV_UART1, &buffer[i], 1);
        } else if(buffer[i] == '\r') {
            print_str("\\r");
        } else if(buffer[i] == '\n') {
            print_str("\\n\r\n");  /* 换行时也在终端换行 */
        } else {
            /* 不可打印字符显示为十六进制 */
            print_str("[");
            print_hex_byte(buffer[i]);
            print_str("]");
        }
        uart_poll_dma_tx(DEV_UART1);
    }
    
    print_str("\r\n--------------------\r\n");
    print_str("Total received: ");
    print_dec(total);
    print_str(" bytes\r\n");
    
    /* 尝试解析+IPD数据 */
    char *ipd_ptr = strstr((char*)buffer, "+IPD,");
    if(ipd_ptr != NULL) {
        print_str("\r\n5. Parsing +IPD data...\r\n");
        ipd_ptr += 5;  /* 跳过"+IPD," */
        uint16_t data_len = atoi(ipd_ptr);
        print_str("IPD data length: ");
        print_dec(data_len);
        print_str(" bytes\r\n");
        
        /* 找到数据起始位置（冒号后） */
        char *data_start = strchr(ipd_ptr, ':');
        if(data_start != NULL) {
            data_start++;  /* 跳过冒号 */
            
            /* 检查HTTP响应状态 */
            if(strstr(data_start, "HTTP/1.") != NULL) {
                print_str("Found HTTP response header\r\n");
                
                /* 查找状态码 */
                char *status = strstr(data_start, "HTTP/1.");
                if(status != NULL) {
                    status = strchr(status, ' ');
                    if(status != NULL) {
                        int code = atoi(status + 1);
                        print_str("HTTP Status Code: ");
                        print_dec(code);
                        print_str("\r\n");
                    }
                }
                
                /* 查找Content-Length */
                char *content_len = strstr(data_start, "Content-Length:");
                if(content_len != NULL) {
                    content_len += 15;
                    uint32_t size = atoi(content_len);
                    print_str("Content-Length: ");
                    print_dec(size);
                    print_str(" bytes\r\n");
                }
            }
        }
    } else {
        print_str("\r\n5. No +IPD header found in response\r\n");
        print_str("This might be a problem with ESP8266 firmware\r\n");
    }
    
    /* 关闭连接 */
    print_str("\r\n6. Closing connection...\r\n");
    esp_send_command("AT+CIPCLOSE", NULL, 2000);
    
    print_str("\r\nDebug complete!\r\n");
    
    /* 建议 */
    print_str("\r\n=== Suggestions ===\r\n");
    print_str("1. Check if ESP8266 firmware supports HTTP properly\r\n");
    print_str("2. Try updating ESP8266 firmware if needed\r\n");
    print_str("3. The server is responding (check server logs)\r\n");
    print_str("4. Issue might be in +IPD data parsing\r\n");
}