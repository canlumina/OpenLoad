#ifndef __HTTP_OTA_H__
#define __HTTP_OTA_H__

#include "http_client.h"
#include "esp8266_wifi.h"
#include "bootloader_cmd.h"
#include <stdint.h>
#include <stdbool.h>

/* OTA状态 */
typedef enum {
    OTA_STATUS_OK = 0,
    OTA_STATUS_ERROR,
    OTA_STATUS_TIMEOUT,
    OTA_STATUS_WIFI_ERROR,
    OTA_STATUS_HTTP_ERROR,
    OTA_STATUS_FLASH_ERROR,
    OTA_STATUS_SIZE_ERROR
} ota_status_t;

/* OTA目标类型 */
typedef enum {
    OTA_TARGET_INTERNAL_FLASH = 0,
    OTA_TARGET_EXTERNAL_FLASH
} ota_target_t;

/* OTA进度回调函数类型 */
typedef void (*ota_progress_callback_t)(uint32_t current, uint32_t total);

/* OTA上下文 */
typedef struct {
    esp8266_device_t *esp8266;
    http_client_t http_client;
    
    /* OTA参数 */
    ota_target_t target;
    uint32_t target_addr;       /* 目标地址（内部Flash地址或外部Flash分区） */
    uint32_t max_size;          /* 最大允许大小 */
    bool is_encrypted;          /* 是否为加密固件 */
    char encryption_password[32]; /* 加密密码（从HTTP响应头获取） */
    
    /* 进度信息 */
    uint32_t total_size;        /* 固件总大小 */
    uint32_t downloaded_size;   /* 已下载大小 */
    ota_progress_callback_t progress_callback;
    
    /* 数据缓冲区 */
    uint8_t *buffer;
    uint16_t buffer_size;
    uint16_t buffer_used;
} ota_context_t;

/* HTTP OTA API */
bool ota_init(ota_context_t *ctx, esp8266_device_t *esp8266);
ota_status_t ota_download_firmware(ota_context_t *ctx, const char *url, 
                                  ota_target_t target, uint32_t target_addr, uint32_t max_size,
                                  bool is_encrypted);
void ota_set_progress_callback(ota_context_t *ctx, ota_progress_callback_t callback);
bool ota_deinit(ota_context_t *ctx);

/* WiFi配置 */
#define OTA_WIFI_SSID           "YANG"
#define OTA_WIFI_PASSWORD       "yang123456789"

#endif /* __HTTP_OTA_H__ */