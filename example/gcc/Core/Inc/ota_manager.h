#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

/* OTA服务器配置 */
#define OTA_SERVER_HOST    "115.190.137.231"
#define OTA_SERVER_PORT    3685
#define OTA_SERVER_URL     "http://115.190.137.231:3685"

/* OTA状态 */
typedef enum {
    OTA_STATE_IDLE,
    OTA_STATE_CHECKING,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_VERIFYING,
    OTA_STATE_COMPLETE,
    OTA_STATE_ERROR
} ota_state_t;

/* OTA目标 */
typedef enum {
    OTA_TARGET_INTERNAL_FLASH,
    OTA_TARGET_EXTERNAL_FLASH
} ota_target_t;

/* OTA固件信息 */
typedef struct {
    char version[32];
    uint32_t size;
    char md5[33];
    char description[128];
} ota_firmware_info_t;

/* OTA管理器配置 */
typedef struct {
    ota_target_t target;
    uint8_t external_slot;     /* 外部Flash槽位(1-3) */
    bool auto_retry;           /* 自动重试 */
    uint8_t retry_count;       /* 重试次数 */
    uint32_t chunk_size;       /* 分块大小 */
} ota_config_t;

/* OTA进度回调 */
typedef void (*ota_progress_callback_t)(uint32_t current, uint32_t total, uint8_t percent);

/* 函数声明 */
bool ota_manager_init(void);
bool ota_manager_check_update(ota_firmware_info_t *info);
bool ota_manager_get_firmware_list(void);
bool ota_manager_download_firmware(const char *version, ota_config_t *config, ota_progress_callback_t callback);
bool ota_manager_verify_firmware(ota_target_t target, uint8_t slot);
ota_state_t ota_manager_get_state(void);
const char* ota_manager_get_error(void);

/* 辅助函数 */
bool ota_parse_json_response(const char *json, ota_firmware_info_t *info);
bool ota_calculate_md5(uint32_t addr, uint32_t size, char *md5);

#endif