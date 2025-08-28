#include "ota_manager.h"
#include "esp8266.h"
#include "bootloader_cmd.h"
#include "w25q64.h"
#include "dev_usart.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* 私有变量 */
static ota_state_t ota_state = OTA_STATE_IDLE;
static char error_msg[128] = {0};
static ota_firmware_info_t latest_firmware = {0};

/* 私有函数 */
static void print_str(const char* str)
{
    uart_write(DEV_UART1, (uint8_t*)str, strlen(str));
    uart_poll_dma_tx(DEV_UART1);
}

static void print_dec(uint32_t val)
{
    char buf[12];
    snprintf(buf, sizeof(buf), "%lu", val);
    print_str(buf);
}

/**
 * @brief 初始化OTA管理器
 */
bool ota_manager_init(void)
{
    ota_state = OTA_STATE_IDLE;
    memset(error_msg, 0, sizeof(error_msg));
    return true;
}

/**
 * @brief 检查固件更新
 */
bool ota_manager_check_update(ota_firmware_info_t *info)
{
    char url[256];
    http_response_t response;
    uint8_t buffer[512];
    uint16_t received;
    esp_result_t result;
    
    if(!esp_wifi_is_connected()) {
        strcpy(error_msg, "WiFi not connected");
        return false;
    }
    
    ota_state = OTA_STATE_CHECKING;
    
    /* 构建URL */
    snprintf(url, sizeof(url), "%s/api/firmware/latest", OTA_SERVER_URL);
    
    /* 发起HTTP请求 */
    result = esp_http_get(url, &response);
    if(result != ESP_OK) {
        strcpy(error_msg, "HTTP request failed");
        ota_state = OTA_STATE_ERROR;
        return false;
    }
    
    /* 检查HTTP状态码 */
    if(response.status_code != 200) {
        snprintf(error_msg, sizeof(error_msg), "HTTP error %d", response.status_code);
        esp_http_close();
        ota_state = OTA_STATE_ERROR;
        return false;
    }
    
    /* 读取JSON响应 */
    memset(buffer, 0, sizeof(buffer));
    received = esp_http_read_data(buffer, sizeof(buffer) - 1);
    esp_http_close();
    
    if(received == 0) {
        strcpy(error_msg, "Empty response");
        ota_state = OTA_STATE_ERROR;
        return false;
    }
    
    /* 解析JSON响应 */
    if(!ota_parse_json_response((char*)buffer, &latest_firmware)) {
        strcpy(error_msg, "JSON parse error");
        ota_state = OTA_STATE_ERROR;
        return false;
    }
    
    if(info != NULL) {
        memcpy(info, &latest_firmware, sizeof(ota_firmware_info_t));
    }
    
    ota_state = OTA_STATE_IDLE;
    return true;
}

/**
 * @brief 获取固件列表
 */
bool ota_manager_get_firmware_list(void)
{
    char url[256];
    http_response_t response;
    uint8_t buffer[1024];
    uint16_t received;
    esp_result_t result;
    
    if(!esp_wifi_is_connected()) {
        strcpy(error_msg, "WiFi not connected");
        return false;
    }
    
    /* 构建URL */
    snprintf(url, sizeof(url), "%s/api/firmware/list", OTA_SERVER_URL);
    
    /* 发起HTTP请求 */
    result = esp_http_get(url, &response);
    if(result != ESP_OK) {
        strcpy(error_msg, "HTTP request failed");
        return false;
    }
    
    /* 检查HTTP状态码 */
    if(response.status_code != 200) {
        esp_http_close();
        return false;
    }
    
    print_str("\r\n=== Available Firmware ===\r\n");
    
    /* 读取并显示响应 */
    while((received = esp_http_read_data(buffer, sizeof(buffer) - 1)) > 0) {
        buffer[received] = '\0';
        
        /* 简单解析并显示版本信息 */
        char *ptr = strstr((char*)buffer, "\"version\":");
        while(ptr != NULL) {
            ptr += 11; /* 跳过 "version": */
            char *end = strchr(ptr, '"');
            if(end != NULL) {
                ptr = strchr(ptr, '"');
                if(ptr != NULL) {
                    ptr++;
                    end = strchr(ptr, '"');
                    if(end != NULL) {
                        *end = '\0';
                        print_str("Version: ");
                        print_str(ptr);
                        
                        /* 查找大小 */
                        ptr = strstr(end + 1, "\"size\":");
                        if(ptr != NULL) {
                            ptr += 7;
                            uint32_t size = atoi(ptr);
                            print_str(" (");
                            print_dec(size / 1024);
                            print_str(" KB)\r\n");
                        }
                    }
                }
            }
            
            ptr = strstr(end + 1, "\"version\":");
        }
    }
    
    esp_http_close();
    return true;
}

/**
 * @brief 下载固件
 */
bool ota_manager_download_firmware(const char *version, ota_config_t *config, ota_progress_callback_t callback)
{
    char url[256];
    ota_info_t ota_info;
    uint8_t *buffer;
    uint16_t received;
    uint32_t total_written = 0;
    esp_result_t result;
    bool success = false;
    
    if(!esp_wifi_is_connected()) {
        strcpy(error_msg, "WiFi not connected");
        return false;
    }
    
    if(config == NULL) {
        strcpy(error_msg, "Invalid config");
        return false;
    }
    
    ota_state = OTA_STATE_DOWNLOADING;
    
    /* 分配缓冲区 */
    uint32_t chunk_size = (config->chunk_size > 0) ? config->chunk_size : 1024;
    buffer = (uint8_t*)malloc(chunk_size);
    if(buffer == NULL) {
        strcpy(error_msg, "Memory allocation failed");
        ota_state = OTA_STATE_ERROR;
        return false;
    }
    
    /* 构建下载URL */
    if(version == NULL || strcmp(version, "latest") == 0) {
        snprintf(url, sizeof(url), "%s/api/firmware/download/latest", OTA_SERVER_URL);
    } else {
        snprintf(url, sizeof(url), "%s/api/firmware/download/%s", OTA_SERVER_URL, version);
    }
    
    /* 开始OTA下载 */
    uint8_t retry = 0;
    do {
        result = esp_ota_start(url, &ota_info);
        if(result == ESP_OK) {
            break;
        }
        
        if(!config->auto_retry || retry >= config->retry_count) {
            strcpy(error_msg, "Failed to start OTA");
            goto cleanup;
        }
        
        retry++;
        print_str("Retrying...\r\n");
        HAL_Delay(2000);
    } while(retry < config->retry_count);
    
    /* 根据目标擦除Flash */
    if(config->target == OTA_TARGET_INTERNAL_FLASH) {
        if(!bootloader_flash_erase(APP_START_ADDR, APP_MAX_SIZE)) {
            strcpy(error_msg, "Flash erase failed");
            esp_ota_finish();
            goto cleanup;
        }
    } else {
        w25q64_partition_id_t pid = W25Q64_PARTITION_BACKUP1 + config->external_slot - 1;
        w25q64_init();
        if(!w25q64_erase_partition(pid)) {
            strcpy(error_msg, "External flash erase failed");
            esp_ota_finish();
            goto cleanup;
        }
    }
    
    /* 下载并写入固件 */
    while((received = esp_ota_read(buffer, chunk_size)) > 0) {
        if(config->target == OTA_TARGET_INTERNAL_FLASH) {
            if(!bootloader_flash_write(APP_START_ADDR + total_written, buffer, received)) {
                strcpy(error_msg, "Flash write failed");
                esp_ota_finish();
                goto cleanup;
            }
        } else {
            w25q64_partition_id_t pid = W25Q64_PARTITION_BACKUP1 + config->external_slot - 1;
            if(!w25q64_write_partition(pid, total_written, buffer, received)) {
                strcpy(error_msg, "External flash write failed");
                esp_ota_finish();
                goto cleanup;
            }
        }
        
        total_written += received;
        
        /* 调用进度回调 */
        if(callback != NULL) {
            uint8_t percent = (ota_info.total_size > 0) ? 
                             ((total_written * 100) / ota_info.total_size) : 0;
            callback(total_written, ota_info.total_size, percent);
        }
        
        /* 检查超时或错误 */
        if(!esp_tcp_is_connected() && received == 0) {
            if(config->auto_retry && retry < config->retry_count) {
                retry++;
                print_str("\r\nConnection lost, retrying...\r\n");
                esp_ota_finish();
                
                /* 重新连接并继续下载 */
                result = esp_ota_start(url, &ota_info);
                if(result != ESP_OK) {
                    strcpy(error_msg, "Reconnection failed");
                    goto cleanup;
                }
                
                /* TODO: 实现断点续传 */
            } else {
                strcpy(error_msg, "Connection lost");
                esp_ota_finish();
                goto cleanup;
            }
        }
    }
    
    /* 完成OTA */
    esp_ota_finish();
    
    /* 验证固件 */
    ota_state = OTA_STATE_VERIFYING;
    if(!ota_manager_verify_firmware(config->target, config->external_slot)) {
        strcpy(error_msg, "Firmware verification failed");
        ota_state = OTA_STATE_ERROR;
        goto cleanup;
    }
    
    ota_state = OTA_STATE_COMPLETE;
    success = true;
    
cleanup:
    if(buffer != NULL) {
        free(buffer);
    }
    if(!success) {
        ota_state = OTA_STATE_ERROR;
    }
    return success;
}

/**
 * @brief 验证固件
 */
bool ota_manager_verify_firmware(ota_target_t target, uint8_t slot)
{
    if(target == OTA_TARGET_INTERNAL_FLASH) {
        return bootloader_validate_app();
    } else {
        uint8_t verify_buf[256];
        w25q64_partition_id_t pid = W25Q64_PARTITION_BACKUP1 + slot - 1;
        
        if(!w25q64_read_partition(pid, 0, verify_buf, 256)) {
            return false;
        }
        
        /* 检查栈指针 */
        uint32_t stack = *((uint32_t*)verify_buf);
        if(stack >= 0x20000000 && stack <= 0x20010000) {
            return true;
        }
    }
    
    return false;
}

/**
 * @brief 获取OTA状态
 */
ota_state_t ota_manager_get_state(void)
{
    return ota_state;
}

/**
 * @brief 获取错误信息
 */
const char* ota_manager_get_error(void)
{
    return error_msg;
}

/**
 * @brief 解析JSON响应
 */
bool ota_parse_json_response(const char *json, ota_firmware_info_t *info)
{
    char *ptr;
    
    if(json == NULL || info == NULL) {
        return false;
    }
    
    memset(info, 0, sizeof(ota_firmware_info_t));
    
    /* 解析版本 */
    ptr = strstr(json, "\"version\":");
    if(ptr != NULL) {
        ptr = strchr(ptr, '"');
        if(ptr != NULL) {
            ptr = strchr(ptr + 1, '"');
            if(ptr != NULL) {
                ptr++;
                char *end = strchr(ptr, '"');
                if(end != NULL) {
                    size_t len = end - ptr;
                    if(len < sizeof(info->version)) {
                        strncpy(info->version, ptr, len);
                    }
                }
            }
        }
    }
    
    /* 解析大小 */
    ptr = strstr(json, "\"size\":");
    if(ptr != NULL) {
        ptr += 7;
        info->size = atoi(ptr);
    }
    
    /* 解析MD5 */
    ptr = strstr(json, "\"md5\":");
    if(ptr != NULL) {
        ptr = strchr(ptr, '"');
        if(ptr != NULL) {
            ptr = strchr(ptr + 1, '"');
            if(ptr != NULL) {
                ptr++;
                char *end = strchr(ptr, '"');
                if(end != NULL) {
                    size_t len = end - ptr;
                    if(len == 32) {
                        strncpy(info->md5, ptr, 32);
                    }
                }
            }
        }
    }
    
    /* 解析描述 */
    ptr = strstr(json, "\"description\":");
    if(ptr != NULL) {
        ptr = strchr(ptr, '"');
        if(ptr != NULL) {
            ptr = strchr(ptr + 1, '"');
            if(ptr != NULL) {
                ptr++;
                char *end = strchr(ptr, '"');
                if(end != NULL) {
                    size_t len = end - ptr;
                    if(len < sizeof(info->description)) {
                        strncpy(info->description, ptr, len);
                    }
                }
            }
        }
    }
    
    return (strlen(info->version) > 0);
}