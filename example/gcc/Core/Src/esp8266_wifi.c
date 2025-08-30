#include "esp8266_wifi.h"
#include "dev_usart.h"
#include "gpio.h"
#include "main.h"
#include <string.h>
#include <stdio.h>

/* ESP8266复位引脚控制 */
static void esp8266_hw_reset(uint8_t reset_pin)
{
    if (reset_pin == 9) {  /* PE9 */
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
        HAL_Delay(100);
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
        HAL_Delay(2000);  /* 等待ESP8266启动 */
    }
}

bool esp8266_init(esp8266_device_t *device, uint8_t uart_dev, uint8_t reset_pin)
{
    if (!device) return false;
    
    /* 初始化AT客户端 */
    if (!at_client_init(&device->at_client, uart_dev)) {
        return false;
    }
    
    /* 硬件复位 */
    esp8266_hw_reset(reset_pin);
    
    /* 测试AT命令 */
    if (at_send_cmd(&device->at_client, "AT", "OK", 3000) != AT_STATUS_OK) {
        return false;
    }
    
    /* 设置WiFi模式为Station */
    if (at_send_cmd(&device->at_client, "AT+CWMODE=1", "OK", 3000) != AT_STATUS_OK) {
        return false;
    }
    
    /* 禁用回显 */
    at_send_cmd(&device->at_client, "ATE0", "OK", 1000);
    
    device->wifi_status = ESP8266_WIFI_DISCONNECTED;
    device->is_initialized = true;
    memset(device->ip_addr, 0, sizeof(device->ip_addr));
    memset(device->gateway, 0, sizeof(device->gateway));
    memset(device->netmask, 0, sizeof(device->netmask));
    memset(device->ssid, 0, sizeof(device->ssid));
    
    return true;
}

bool esp8266_reset(esp8266_device_t *device)
{
    if (!device || !device->is_initialized) return false;
    
    /* 软件复位 */
    if (at_send_cmd(&device->at_client, "AT+RST", "ready", 5000) != AT_STATUS_OK) {
        return false;
    }
    
    HAL_Delay(1000);
    
    /* 重新配置 */
    at_send_cmd(&device->at_client, "AT+CWMODE=1", "OK", 3000);
    at_send_cmd(&device->at_client, "ATE0", "OK", 1000);
    
    device->wifi_status = ESP8266_WIFI_DISCONNECTED;
    
    return true;
}

bool esp8266_connect_wifi(esp8266_device_t *device, const char *ssid, const char *password)
{
    if (!device || !device->is_initialized || !ssid || !password) return false;
    
    char cmd[128];
    
    /* 断开现有连接 */
    at_send_cmd(&device->at_client, "AT+CWQAP", "OK", 3000);
    
    /* 连接WiFi */
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    
    if (at_send_cmd(&device->at_client, cmd, "WIFI GOT IP", 15000) != AT_STATUS_OK) {
        device->wifi_status = ESP8266_WIFI_ERROR;
        return false;
    }
    
    /* 保存SSID */
    strncpy(device->ssid, ssid, sizeof(device->ssid) - 1);
    device->wifi_status = ESP8266_WIFI_GOT_IP;
    
    /* 等待一段时间确保IP分配完成 */
    HAL_Delay(1000);
    
    /* 获取IP信息 */
    esp8266_get_ip_info(device);
    
    return true;
}

bool esp8266_disconnect_wifi(esp8266_device_t *device)
{
    if (!device || !device->is_initialized) return false;
    
    if (at_send_cmd(&device->at_client, "AT+CWQAP", "OK", 3000) == AT_STATUS_OK) {
        device->wifi_status = ESP8266_WIFI_DISCONNECTED;
        memset(device->ip_addr, 0, sizeof(device->ip_addr));
        memset(device->gateway, 0, sizeof(device->gateway));
        memset(device->netmask, 0, sizeof(device->netmask));
        memset(device->ssid, 0, sizeof(device->ssid));
        return true;
    }
    
    return false;
}

esp8266_wifi_status_t esp8266_get_wifi_status(esp8266_device_t *device)
{
    if (!device || !device->is_initialized) return ESP8266_WIFI_ERROR;
    
    char resp[256];
    if (at_send_cmd_get_resp(&device->at_client, "AT+CWJAP?", resp, sizeof(resp), 3000) == AT_STATUS_OK) {
        if (strstr(resp, "No AP")) {
            device->wifi_status = ESP8266_WIFI_DISCONNECTED;
        } else if (strstr(resp, "+CWJAP:")) {
            device->wifi_status = ESP8266_WIFI_GOT_IP;
        }
    }
    
    return device->wifi_status;
}

bool esp8266_get_ip_info(esp8266_device_t *device)
{
    if (!device || !device->is_initialized) return false;
    
    char resp[512];
    if (at_send_cmd_get_resp(&device->at_client, "AT+CIFSR", resp, sizeof(resp), 3000) != AT_STATUS_OK) {
        return false;
    }
    
    /* 清空IP地址字段以便调试 */
    memset(device->ip_addr, 0, sizeof(device->ip_addr));
    
    /* 解析IP地址 - 尝试多种可能的格式 */
    const char *ip_start = strstr(resp, "STAIP,\"");
    if (!ip_start) {
        ip_start = strstr(resp, "+CIFSR:STAIP,\"");
        if (ip_start) {
            ip_start += 14;  /* 跳过 "+CIFSR:STAIP,\"" */
        }
    } else {
        ip_start += 7;  /* 跳过 "STAIP,\"" */
    }
    
    if (ip_start) {
        const char *ip_end = strchr(ip_start, '\"');
        if (ip_end && (ip_end - ip_start) < sizeof(device->ip_addr) && (ip_end - ip_start) > 0) {
            memcpy(device->ip_addr, ip_start, ip_end - ip_start);
            device->ip_addr[ip_end - ip_start] = '\0';
        }
    }
    
    /* 获取网关和子网掩码 */
    if (at_send_cmd_get_resp(&device->at_client, "AT+CIPSTA?", resp, sizeof(resp), 3000) == AT_STATUS_OK) {
        /* 解析网关 */
        const char *gw_start = strstr(resp, "gateway:\"");
        if (gw_start) {
            gw_start += 9;
            const char *gw_end = strchr(gw_start, '\"');
            if (gw_end && (gw_end - gw_start) < sizeof(device->gateway)) {
                memcpy(device->gateway, gw_start, gw_end - gw_start);
                device->gateway[gw_end - gw_start] = '\0';
            }
        }
        
        /* 解析子网掩码 */
        const char *nm_start = strstr(resp, "netmask:\"");
        if (nm_start) {
            nm_start += 9;
            const char *nm_end = strchr(nm_start, '\"');
            if (nm_end && (nm_end - nm_start) < sizeof(device->netmask)) {
                memcpy(device->netmask, nm_start, nm_end - nm_start);
                device->netmask[nm_end - nm_start] = '\0';
            }
        }
    }
    
    return true;
}

bool esp8266_tcp_connect(esp8266_device_t *device, const char *host, uint16_t port)
{
    if (!device || !device->is_initialized || !host) return false;
    
    char cmd[128];
    
    /* 设置单连接模式 */
    at_send_cmd(&device->at_client, "AT+CIPMUX=0", "OK", 3000);
    
    /* 尝试设置更大的接收缓冲区（某些固件支持） */
    at_send_cmd(&device->at_client, "AT+CIPRECVLEN=2048", "OK", 1000);
    
    /* 建立TCP连接 */
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d", host, port);
    
    if (at_send_cmd(&device->at_client, cmd, "CONNECT", 10000) != AT_STATUS_OK) {
        return false;
    }
    
    return true;
}

bool esp8266_tcp_disconnect(esp8266_device_t *device)
{
    if (!device || !device->is_initialized) return false;
    
    return (at_send_cmd(&device->at_client, "AT+CIPCLOSE", "CLOSED", 5000) == AT_STATUS_OK);
}

int esp8266_tcp_send(esp8266_device_t *device, const uint8_t *data, uint16_t len)
{
    if (!device || !device->is_initialized || !data || len == 0) return -1;
    
    char cmd[32];
    
    /* 发送数据长度命令 */
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", len);
    
    if (at_send_cmd(&device->at_client, cmd, ">", 3000) != AT_STATUS_OK) {
        return -1;
    }
    
    /* 发送实际数据 */
    uart_write(device->at_client.uart_dev, data, len);
    uart_poll_dma_tx(device->at_client.uart_dev);
    
    /* 等待发送完成 */
    if (at_send_cmd(&device->at_client, "", "SEND OK", 10000) != AT_STATUS_OK) {
        return -1;
    }
    
    return len;
}

int esp8266_tcp_receive(esp8266_device_t *device, uint8_t *buffer, uint16_t buffer_size, uint32_t timeout)
{
    if (!device || !device->is_initialized || !buffer) return -1;
    
    uint32_t start_time = HAL_GetTick();
    uint16_t received = 0;
    uint32_t no_data_time = HAL_GetTick();
    
    /* 尝试批量读取以提高效率 */
    while ((HAL_GetTick() - start_time) < timeout && received < buffer_size) {
        /* 尝试一次读取更多数据 */
        uint16_t to_read = buffer_size - received;
        if (to_read > 256) to_read = 256; /* 限制单次读取量 */
        
        int read_count = uart_read(device->at_client.uart_dev, buffer + received, to_read);
        
        if (read_count > 0) {
            received += read_count;
            no_data_time = HAL_GetTick(); /* 重置无数据时间 */
        } else {
            /* 如果超过500ms没有数据且已收到一些数据，可能连接关闭 */
            if (received > 0 && (HAL_GetTick() - no_data_time) > 500) {
                break; /* 提前退出，避免不必要等待 */
            }
            HAL_Delay(1);
        }
    }
    
    return received;
}