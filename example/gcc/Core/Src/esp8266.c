#include <string.h>
#include <stdio.h>
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