#ifndef __OTA_H__
#define __OTA_H__

#include <stdint.h>
#include <stdbool.h>

// OTA配置
#define OTA_SERVER_IP           "115.190.137.231"
#define OTA_SERVER_PORT         3685
#define OTA_DOWNLOAD_TIMEOUT    30000  // 30秒超时
#define OTA_BUFFER_SIZE         512    // 下载缓冲区大小
#define OTA_MAX_RETRY           3      // 最大重试次数

// OTA状态
typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_CONNECTING,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_WRITING,
    OTA_STATE_VERIFYING,
    OTA_STATE_SUCCESS,
    OTA_STATE_FAILED
} ota_state_t;

// OTA错误码
typedef enum {
    OTA_OK = 0,
    OTA_ERR_WIFI_INIT,
    OTA_ERR_WIFI_CONNECT,
    OTA_ERR_SERVER_CONNECT,
    OTA_ERR_HTTP_REQUEST,
    OTA_ERR_DOWNLOAD,
    OTA_ERR_FLASH_WRITE,
    OTA_ERR_VERIFY,
    OTA_ERR_TIMEOUT,
    OTA_ERR_INVALID_PARAM
} ota_error_t;

// OTA信息结构
typedef struct {
    uint32_t file_size;         // 固件大小
    uint32_t downloaded_size;   // 已下载大小
    uint32_t write_addr;        // 写入地址
    uint8_t  progress;          // 下载进度(0-100)
    ota_state_t state;          // 当前状态
    ota_error_t error;          // 错误码
} ota_info_t;

// OTA回调函数
typedef void (*ota_progress_cb_t)(uint8_t progress);
typedef void (*ota_state_cb_t)(ota_state_t state);

// API函数
ota_error_t ota_init(void);
ota_error_t ota_download_firmware(const char *url, uint32_t dest_addr, bool is_internal);
ota_error_t ota_get_info(ota_info_t *info);
void ota_set_progress_callback(ota_progress_cb_t cb);
void ota_set_state_callback(ota_state_cb_t cb);
void ota_abort(void);

#endif /* __OTA_H__ */