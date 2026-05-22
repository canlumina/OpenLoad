#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>
#include <stdbool.h>

/* 配置版本 */
#define CONFIG_VERSION  1

/* WiFi配置 */
typedef struct {
    char ssid[64];          /* WiFi网络名称 */
    char password[64];      /* WiFi密码 */
    uint32_t timeout_ms;    /* 连接超时时间(毫秒) */
} wifi_config_t;

/* OTA服务器配置 */
typedef struct {
    char host[64];          /* 服务器域名或IP */
    uint16_t port;          /* 服务器端口 */
    char path[128];         /* 固件文件路径 */
    uint32_t timeout_ms;    /* 下载超时时间(毫秒) */
} ota_server_config_t;

/* 系统配置 */
typedef struct {
    uint32_t bootloader_delay_ms;   /* Bootloader等待时间(毫秒) */
    uint32_t uart_baudrate;         /* 调试串口波特率 */
    bool auto_ota_enable;           /* 自动OTA使能 */
    uint8_t max_retry_count;        /* 最大重试次数 */
} system_config_t;

/* 总配置结构 */
typedef struct {
    uint32_t version;               /* 配置版本 */
    uint32_t crc32;                /* CRC32校验码 */
    wifi_config_t wifi;             /* WiFi配置 */
    ota_server_config_t ota;        /* OTA配置 */
    system_config_t system;         /* 系统配置 */
} bootloader_config_t;

/* 配置管理函数 */
bool config_init(void);
bool config_load_default(void);
bool config_save(void);
bool config_load(void);
const bootloader_config_t* config_get(void);
bool config_set_wifi(const char* ssid, const char* password);
bool config_set_ota_server(const char* host, uint16_t port, const char* path);
bool config_validate(void);

#endif /* __CONFIG_H__ */