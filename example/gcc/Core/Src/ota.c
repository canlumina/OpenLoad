#include "ota.h"
#include "wifi.h"
#include "w25q64.h"
#include "main.h"
#include "bootloader_cmd.h"  // 为了使用print函数
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// 内部Flash定义
#define INTERNAL_FLASH_BASE     0x08000000

// 外部Flash定义  
#define EXTERNAL_FLASH_BASE     0x00000000
#define EXTERNAL_APP_SIZE       (2 * 1024 * 1024)  // 2MB

static ota_info_t ota_info = {0};
static ota_progress_cb_t progress_callback = NULL;
static ota_state_cb_t state_callback = NULL;
static bool ota_abort_flag = false;

// 更新OTA状态
static void ota_set_state(ota_state_t state)
{
    ota_info.state = state;
    if(state_callback) {
        state_callback(state);
    }
}

// 更新下载进度
static void ota_update_progress(void)
{
    if(ota_info.file_size > 0) {
        ota_info.progress = (ota_info.downloaded_size * 100) / ota_info.file_size;
        if(progress_callback) {
            progress_callback(ota_info.progress);
        }
    }
}

// 擦除内部Flash
static bool erase_internal_flash(uint32_t addr, uint32_t size)
{
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError = 0;
    
    HAL_FLASH_Unlock();
    
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = addr;
    EraseInitStruct.NbPages = (size + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE;
    
    if(HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) != HAL_OK) {
        HAL_FLASH_Lock();
        return false;
    }
    
    HAL_FLASH_Lock();
    return true;
}

// 写入内部Flash
static bool write_internal_flash(uint32_t addr, const uint8_t *data, uint32_t len)
{
    HAL_FLASH_Unlock();
    
    // 按字写入(4字节对齐)
    uint32_t *src = (uint32_t*)data;
    uint32_t words = len / 4;
    
    for(uint32_t i = 0; i < words; i++) {
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i*4, src[i]) != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
    }
    
    // 处理剩余字节
    uint32_t remain = len % 4;
    if(remain > 0) {
        uint32_t last_word = 0xFFFFFFFF;
        memcpy(&last_word, data + words*4, remain);
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + words*4, last_word) != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
    }
    
    HAL_FLASH_Lock();
    return true;
}

// 解析HTTP响应头
static int32_t parse_http_header(const char *response, uint32_t *content_length)
{
    const char *ptr;
    
    bootloader_print("OTA: Parsing HTTP header...\r\n");
    
    // 检查HTTP状态码
    if(strncmp(response, "HTTP/1", 6) != 0) {
        bootloader_print("OTA: Invalid HTTP response header\r\n");
        return -1;
    }
    
    ptr = strchr(response, ' ');
    if(!ptr) {
        bootloader_print("OTA: No status code found\r\n");
        return -1;
    }
    
    int status_code = atoi(ptr + 1);
    bootloader_print("OTA: HTTP status code: ");
    bootloader_print_dec(status_code);
    bootloader_print("\r\n");
    
    if(status_code != 200) {
        bootloader_print("OTA: HTTP error status\r\n");
        return -1;
    }
    
    // 获取Content-Length
    *content_length = 0;
    ptr = strstr(response, "Content-Length:");
    if(ptr) {
        *content_length = atoi(ptr + 15);
        bootloader_print("OTA: Content-Length: ");
        bootloader_print_dec(*content_length);
        bootloader_print("\r\n");
    } else {
        // 检查Transfer-Encoding: chunked
        ptr = strstr(response, "Transfer-Encoding: chunked");
        if(ptr) {
            bootloader_print("OTA: Using chunked transfer encoding\r\n");
            *content_length = 0;  // 会在后续处理中计算
        } else {
            bootloader_print("OTA: Warning - No Content-Length or Transfer-Encoding found\r\n");
        }
    }
    
    // 检查Connection头
    ptr = strstr(response, "Connection:");
    if(ptr) {
        if(strstr(ptr, "close")) {
            bootloader_print("OTA: Server will close connection after response\r\n");
        } else if(strstr(ptr, "keep-alive")) {
            bootloader_print("OTA: Server will keep connection alive\r\n");
        }
    }
    
    // 找到响应体起始位置
    ptr = strstr(response, "\r\n\r\n");
    if(ptr) {
        int header_len = (ptr + 4 - response);
        bootloader_print("OTA: Header length: ");
        bootloader_print_dec(header_len);
        bootloader_print("\r\n");
        return header_len;
    }
    
    bootloader_print("OTA: Header end not found\r\n");
    return -1;
}

// 下载固件
static ota_error_t download_firmware(const char *url, uint32_t dest_addr, bool is_internal)
{
    static uint8_t header_buffer[1024];  // 用于接收完整HTTP头
    uint8_t buffer[OTA_BUFFER_SIZE];
    char http_request[512];
    uint32_t total_size = 0;
    uint32_t received = 0;
    int32_t header_offset = -1;
    uint32_t header_len = 0;
    
    bootloader_print("OTA: Building HTTP request for: ");
    bootloader_print(url);
    bootloader_print("\r\n");
    
    // 构建HTTP GET请求
    snprintf(http_request, sizeof(http_request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "User-Agent: STM32-Bootloader\r\n"
             "Connection: close\r\n"
             "\r\n",
             url, OTA_SERVER_IP, OTA_SERVER_PORT);
    
    // 发送HTTP请求
    bootloader_print("OTA: Sending HTTP request (length: ");
    bootloader_print_dec(strlen(http_request));
    bootloader_print(" bytes)\r\n");
    
    // 打印请求内容（调试用）
    bootloader_print("OTA: Request content:\r\n");
    bootloader_print(http_request);
    bootloader_print("\r\n");
    
    int32_t send_result = wifi_tcp_send((uint8_t*)http_request, strlen(http_request));
    if(send_result < 0) {
        bootloader_print("OTA: Failed to send HTTP request\r\n");
        return OTA_ERR_HTTP_REQUEST;
    }
    bootloader_print("OTA: HTTP request sent successfully\r\n");
    
    // 接收完整的HTTP响应头
    bootloader_print("OTA: Waiting for HTTP response...\r\n");
    
    memset(header_buffer, 0, sizeof(header_buffer));
    
    // 逐步接收数据直到找到完整的HTTP头
    while(header_len < sizeof(header_buffer) - 1) {
        int32_t len = wifi_tcp_recv(buffer, sizeof(buffer), 3000);
        if(len <= 0) {
            bootloader_print("OTA: No more data from server\r\n");
            break;
        }
        
        // 检查是否超出缓冲区
        if(header_len + len >= sizeof(header_buffer)) {
            len = sizeof(header_buffer) - header_len - 1;
        }
        
        // 复制数据到头缓冲区
        memcpy(header_buffer + header_len, buffer, len);
        header_len += len;
        header_buffer[header_len] = '\0';
        
        bootloader_print("OTA: Received ");
        bootloader_print_dec(len);
        bootloader_print(" bytes (total: ");
        bootloader_print_dec(header_len);
        bootloader_print(")\r\n");
        
        // 检查是否已收到完整的HTTP头
        char *header_end = strstr((char*)header_buffer, "\r\n\r\n");
        if(header_end) {
            bootloader_print("OTA: Found complete HTTP header\r\n");
            break;
        }
    }
    
    if(header_len == 0) {
        bootloader_print("OTA: No response from server\r\n");
        return OTA_ERR_DOWNLOAD;
    }
    
    // 打印响应内容（前200字节，同时显示十六进制和ASCII）
    bootloader_print("OTA: Response preview (first 100 bytes):\r\n");
    bootloader_print("HEX: ");
    for(uint32_t i = 0; i < (header_len > 100 ? 100 : header_len); i++) {
        char hex_str[4];
        snprintf(hex_str, sizeof(hex_str), "%02X ", header_buffer[i]);
        bootloader_print(hex_str);
        if((i + 1) % 16 == 0) bootloader_print("\r\n     ");
    }
    bootloader_print("\r\nASCII: ");
    for(uint32_t i = 0; i < (header_len > 100 ? 100 : header_len); i++) {
        if(header_buffer[i] >= 32 && header_buffer[i] <= 126) {  // 可打印字符
            char c[2] = {header_buffer[i], '\0'};
            bootloader_print(c);
        } else {
            bootloader_print(".");
        }
    }
    bootloader_print("\r\n");
    
    // 检查是否为HTTP响应还是直接的二进制数据
    bool is_http_response = (strncmp((char*)header_buffer, "HTTP/", 5) == 0);
    
    if(is_http_response) {
        // 解析HTTP响应头
        header_offset = parse_http_header((char*)header_buffer, &total_size);
        if(header_offset < 0 || total_size == 0) {
            bootloader_print("OTA: Failed to parse HTTP header or invalid content length\r\n");
            return OTA_ERR_DOWNLOAD;
        }
        bootloader_print("OTA: HTTP response detected\r\n");
    } else {
        // 直接的二进制固件数据
        bootloader_print("OTA: Binary firmware data detected\r\n");
        header_offset = 0;  // 没有HTTP头
        total_size = header_len;  // 假设收到的是完整固件的一部分
        
        // 尝试从文件头推断固件大小（STM32向量表的特征）
        if(header_len >= 8) {
            // 检查STM32固件的特征：前4字节是栈指针，通常在SRAM范围内
            uint32_t stack_ptr = *(uint32_t*)header_buffer;
            uint32_t reset_handler = *(uint32_t*)(header_buffer + 4);
            
            if((stack_ptr & 0xFFFF0000) == 0x20000000 && // 栈指针在SRAM区域
               (reset_handler & 0xFFFF0000) == 0x08000000) { // 复位向量在Flash区域
                bootloader_print("OTA: Valid STM32 firmware header detected\r\n");
                bootloader_print("OTA: Stack pointer: 0x");
                bootloader_print_hex(stack_ptr);
                bootloader_print(", Reset handler: 0x");
                bootloader_print_hex(reset_handler);
                bootloader_print("\r\n");
                
                // 由于无法从二进制数据确定文件大小，使用默认大小或继续接收直到连接关闭
                total_size = 64 * 1024;  // 假设最大64KB固件
                bootloader_print("OTA: Assuming maximum firmware size: ");
                bootloader_print_dec(total_size);
                bootloader_print(" bytes\r\n");
            }
        }
    }
    
    ota_info.file_size = total_size;
    ota_info.downloaded_size = 0;
    
    bootloader_print("OTA: Firmware size: ");
    bootloader_print_dec(total_size);
    bootloader_print(" bytes\r\n");
    
    // 准备Flash
    if(is_internal) {
        bootloader_print("OTA: Erasing internal flash...\r\n");
        if(!erase_internal_flash(dest_addr, total_size)) {
            bootloader_print("OTA: Flash erase failed\r\n");
            return OTA_ERR_FLASH_WRITE;
        }
    } else {
        // 擦除外部Flash扇区
        uint32_t sectors = (total_size + W25Q64_SECTOR_SIZE - 1) / W25Q64_SECTOR_SIZE;
        bootloader_print("OTA: Erasing external flash ");
        bootloader_print_dec(sectors);
        bootloader_print(" sectors (");
        bootloader_print_dec(sectors * W25Q64_SECTOR_SIZE / 1024);
        bootloader_print("KB)...\r\n");
        
        for(uint32_t i = 0; i < sectors; i++) {
            bootloader_print("OTA: Erasing sector ");
            bootloader_print_dec(i + 1);
            bootloader_print("/");
            bootloader_print_dec(sectors);
            bootloader_print(" at 0x");
            bootloader_print_hex(dest_addr + i * W25Q64_SECTOR_SIZE);
            bootloader_print("...");
            
            if(!w25q64_erase_sector(dest_addr + i * W25Q64_SECTOR_SIZE)) {
                bootloader_print(" FAILED\r\n");
                bootloader_print("OTA: Sector erase failed\r\n");
                return OTA_ERR_FLASH_WRITE;
            }
            bootloader_print(" OK\r\n");
        }
    }
    bootloader_print("OTA: Flash preparation complete\r\n");
    
    // 检查第一包中是否包含固件数据
    uint32_t first_data_len = 0;
    if(is_http_response && header_offset > 0) {
        first_data_len = header_len - header_offset;
    } else if(!is_http_response) {
        first_data_len = header_len;
        header_offset = 0;
    }
    
    bootloader_print("OTA: HTTP header ends at offset ");
    bootloader_print_dec(header_offset);
    bootloader_print(", received total ");
    bootloader_print_dec(header_len);
    bootloader_print(" bytes\r\n");
    
    if(first_data_len > 0) {
        bootloader_print("OTA: Found ");
        bootloader_print_dec(first_data_len);
        bootloader_print(" bytes of firmware data in first packet\r\n");
        
        // 显示前32字节的十六进制内容用于调试
        bootloader_print("OTA: First 32 bytes of firmware data: ");
        for(uint32_t i = 0; i < (first_data_len > 32 ? 32 : first_data_len); i++) {
            char hex_str[4];
            snprintf(hex_str, sizeof(hex_str), "%02X ", header_buffer[header_offset + i]);
            bootloader_print(hex_str);
        }
        bootloader_print("\r\n");
        
        if(is_internal) {
            if(!write_internal_flash(dest_addr, header_buffer + header_offset, first_data_len)) {
                bootloader_print("OTA: Failed to write first chunk to internal flash\r\n");
                return OTA_ERR_FLASH_WRITE;
            }
        } else {
            w25q64_write(dest_addr, header_buffer + header_offset, first_data_len);
        }
        received += first_data_len;
        ota_info.downloaded_size = received;
        ota_update_progress();
        
        bootloader_print("OTA: First chunk written, progress: ");
        bootloader_print_dec(ota_info.progress);
        bootloader_print("%\r\n");
    } else {
        bootloader_print("OTA: No firmware data in first packet, will receive from server\r\n");
    }
    
    // 继续接收剩余数据
    uint32_t timeout_count = 0;
    const uint32_t MAX_TIMEOUT_COUNT = is_http_response ? 10 : 5;  // 二进制模式下减少等待时间
    
    bootloader_print("OTA: Starting to receive remaining data...\r\n");
    
    while(!ota_abort_flag) {
        int32_t len = wifi_tcp_recv(buffer, sizeof(buffer), 5000);
        if(len <= 0) {
            timeout_count++;
            bootloader_print("OTA: Receive timeout (");
            bootloader_print_dec(timeout_count);
            bootloader_print("/");
            bootloader_print_dec(MAX_TIMEOUT_COUNT);
            bootloader_print(")\r\n");
            
            if(timeout_count >= MAX_TIMEOUT_COUNT) {
                bootloader_print("OTA: No more data from server\r\n");
                break;
            }
            continue;
        }
        
        // 重置超时计数
        timeout_count = 0;
        
        // 对于二进制模式，动态调整总大小
        if(!is_http_response && received + len > total_size) {
            total_size = received + len + 4096;  // 预留一些空间
            ota_info.file_size = total_size;
            bootloader_print("OTA: Adjusting total size to ");
            bootloader_print_dec(total_size);
            bootloader_print(" bytes\r\n");
        }
        
        // 写入Flash
        if(is_internal) {
            if(!write_internal_flash(dest_addr + received, buffer, len)) {
                bootloader_print("OTA: Failed to write chunk at offset ");
                bootloader_print_dec(received);
                bootloader_print("\r\n");
                return OTA_ERR_FLASH_WRITE;
            }
        } else {
            w25q64_write(dest_addr + received, buffer, len);
        }
        
        received += len;
        ota_info.downloaded_size = received;
        ota_update_progress();
        
        // 每收到4KB数据打印一次进度
        if(received % 4096 == 0) {
            bootloader_print("OTA: Progress: ");
            bootloader_print_dec(received);
            bootloader_print(" bytes received\r\n");
        }
        
        // 对于HTTP模式，检查是否达到预期大小
        if(is_http_response && received >= total_size) {
            bootloader_print("OTA: Received expected amount of data\r\n");
            break;
        }
    }
    
    if(ota_abort_flag) {
        bootloader_print("OTA: Download aborted by user\r\n");
        return OTA_ERR_DOWNLOAD;
    }
    
    bootloader_print("OTA: Download complete. Received ");
    bootloader_print_dec(received);
    bootloader_print(" bytes\r\n");
    
    // 更新实际文件大小
    ota_info.file_size = received;
    
    // 对于二进制模式，只要接收到数据就认为成功
    // 对于HTTP模式，需要检查是否接收到预期大小
    if(is_http_response) {
        return (received == total_size) ? OTA_OK : OTA_ERR_DOWNLOAD;
    } else {
        return (received > 0) ? OTA_OK : OTA_ERR_DOWNLOAD;
    }
}

ota_error_t ota_init(void)
{
    memset(&ota_info, 0, sizeof(ota_info));
    ota_info.state = OTA_STATE_IDLE;
    ota_abort_flag = false;
    
    bootloader_print("OTA: Initializing WiFi module...\r\n");
    
    // 初始化WiFi模块
    if(!wifi_init()) {
        bootloader_print("OTA: WiFi init failed\r\n");
        ota_info.error = OTA_ERR_WIFI_INIT;
        return OTA_ERR_WIFI_INIT;
    }
    
    bootloader_print("OTA: WiFi module ready\r\n");
    return OTA_OK;
}

ota_error_t ota_download_firmware(const char *url, uint32_t dest_addr, bool is_internal)
{
    ota_error_t ret = OTA_OK;
    
    if(!url || dest_addr == 0) {
        return OTA_ERR_INVALID_PARAM;
    }
    
    // 检查地址范围
    if(is_internal) {
        if(dest_addr < APP_START_ADDR || dest_addr >= (APP_START_ADDR + APP_MAX_SIZE)) {
            return OTA_ERR_INVALID_PARAM;
        }
    }
    
    ota_info.write_addr = dest_addr;
    ota_abort_flag = false;
    
    // 连接WiFi
    ota_set_state(OTA_STATE_CONNECTING);
    bootloader_print("OTA: Connecting to WiFi SSID: ");
    bootloader_print(WIFI_SSID);
    bootloader_print("\r\n");
    
    if(!wifi_connect(WIFI_SSID, WIFI_PASSWORD)) {
        bootloader_print("OTA: WiFi connection failed\r\n");
        ota_info.error = OTA_ERR_WIFI_CONNECT;
        ota_set_state(OTA_STATE_FAILED);
        return OTA_ERR_WIFI_CONNECT;
    }
    
    bootloader_print("OTA: WiFi connected successfully\r\n");
    
    // 连接服务器
    bootloader_print("OTA: Connecting to server ");
    bootloader_print(OTA_SERVER_IP);
    bootloader_print(":");
    bootloader_print_dec(OTA_SERVER_PORT);
    bootloader_print("\r\n");
    
    // 重试连接几次
    int retry = 0;
    bool connected = false;
    
    while(retry < 3 && !connected) {
        if(retry > 0) {
            bootloader_print("OTA: Retrying connection (attempt ");
            bootloader_print_dec(retry + 1);
            bootloader_print(")\r\n");
            HAL_Delay(2000);
        }
        
        if(wifi_tcp_connect(OTA_SERVER_IP, OTA_SERVER_PORT)) {
            connected = true;
            bootloader_print("OTA: Server connected\r\n");
            break;
        }
        retry++;
    }
    
    if(!connected) {
        bootloader_print("OTA: TCP connection failed after retries\r\n");
        ota_info.error = OTA_ERR_SERVER_CONNECT;
        ota_set_state(OTA_STATE_FAILED);
        wifi_disconnect();
        return OTA_ERR_SERVER_CONNECT;
    }
    
    // 下载固件
    ota_set_state(OTA_STATE_DOWNLOADING);
    ret = download_firmware(url, dest_addr, is_internal);
    
    // 断开连接
    wifi_tcp_disconnect();
    wifi_disconnect();
    
    if(ret == OTA_OK) {
        ota_set_state(OTA_STATE_SUCCESS);
    } else {
        ota_info.error = ret;
        ota_set_state(OTA_STATE_FAILED);
    }
    
    return ret;
}

ota_error_t ota_get_info(ota_info_t *info)
{
    if(info) {
        memcpy(info, &ota_info, sizeof(ota_info_t));
        return OTA_OK;
    }
    return OTA_ERR_INVALID_PARAM;
}

void ota_set_progress_callback(ota_progress_cb_t cb)
{
    progress_callback = cb;
}

void ota_set_state_callback(ota_state_cb_t cb)
{
    state_callback = cb;
}

void ota_abort(void)
{
    ota_abort_flag = true;
}