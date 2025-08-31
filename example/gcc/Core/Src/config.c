#include "config.h"
#include "w25q64.h"
#include "bootloader_cmd.h"
#include <string.h>

/* 配置存储在W25Q64的最后一个扇区 */
#define CONFIG_SECTOR_ADDR      0x7F0000    /* 8MB - 64KB */
#define CONFIG_MAGIC            0x424C4346  /* "BLCF" */

/* 全局配置实例 */
static bootloader_config_t g_config;
static bool g_config_loaded = false;

/* 简化的CRC32计算 - 不使用查表法以节省Flash空间 */

static uint32_t crc32_calculate(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFF;
}

bool config_init(void)
{
    /* 初始化W25Q64 */
    w25q64_init();
    
    /* 尝试加载配置 */
    if (!config_load()) {
        bootloader_print("Config load failed, using defaults\r\n");
        config_load_default();
        config_save();  /* 保存默认配置 */
    }
    
    g_config_loaded = true;
    return true;
}

bool config_load_default(void)
{
    memset(&g_config, 0, sizeof(g_config));
    
    g_config.version = CONFIG_VERSION;
    
    /* 默认WiFi配置 */
    strcpy(g_config.wifi.ssid, "OpenLoad_WiFi");
    strcpy(g_config.wifi.password, "12345678");
    g_config.wifi.timeout_ms = 15000;
    
    /* 默认OTA服务器配置 */
    strcpy(g_config.ota.host, "192.168.1.100");
    g_config.ota.port = 80;
    strcpy(g_config.ota.path, "/firmware/app.bin");
    g_config.ota.timeout_ms = 30000;
    
    /* 默认系统配置 */
    g_config.system.bootloader_delay_ms = 3000;
    g_config.system.uart_baudrate = 115200;
    g_config.system.auto_ota_enable = false;
    g_config.system.max_retry_count = 3;
    
    /* 计算CRC */
    g_config.crc32 = crc32_calculate((uint8_t*)&g_config.version, 
                                    sizeof(g_config) - sizeof(g_config.crc32));
    
    return true;
}

bool config_save(void)
{
    if (!g_config_loaded) return false;
    
    /* 重新计算CRC */
    g_config.crc32 = crc32_calculate((uint8_t*)&g_config.version, 
                                    sizeof(g_config) - sizeof(g_config.crc32));
    
    /* 擦除扇区 */
    if (!w25q64_erase_sector(CONFIG_SECTOR_ADDR)) {
        return false;
    }
    
    /* 写入配置 */
    return w25q64_write(CONFIG_SECTOR_ADDR, (uint8_t*)&g_config, sizeof(g_config));
}

bool config_load(void)
{
    /* 从Flash读取配置 */
    w25q64_read(CONFIG_SECTOR_ADDR, (uint8_t*)&g_config, sizeof(g_config));
    
    /* 验证版本 */
    if (g_config.version != CONFIG_VERSION) {
        return false;
    }
    
    /* 验证CRC */
    uint32_t calculated_crc = crc32_calculate((uint8_t*)&g_config.version, 
                                             sizeof(g_config) - sizeof(g_config.crc32));
    if (calculated_crc != g_config.crc32) {
        return false;
    }
    
    return config_validate();
}

const bootloader_config_t* config_get(void)
{
    return g_config_loaded ? &g_config : NULL;
}

bool config_set_wifi(const char* ssid, const char* password)
{
    if (!ssid || !password || !g_config_loaded) return false;
    
    if (strlen(ssid) >= sizeof(g_config.wifi.ssid) || 
        strlen(password) >= sizeof(g_config.wifi.password)) {
        return false;
    }
    
    strcpy(g_config.wifi.ssid, ssid);
    strcpy(g_config.wifi.password, password);
    
    return true;
}

bool config_set_ota_server(const char* host, uint16_t port, const char* path)
{
    if (!host || !path || !g_config_loaded) return false;
    
    if (strlen(host) >= sizeof(g_config.ota.host) || 
        strlen(path) >= sizeof(g_config.ota.path)) {
        return false;
    }
    
    strcpy(g_config.ota.host, host);
    g_config.ota.port = port;
    strcpy(g_config.ota.path, path);
    
    return true;
}

bool config_validate(void)
{
    /* 验证WiFi配置 */
    if (strlen(g_config.wifi.ssid) == 0 || strlen(g_config.wifi.ssid) >= 64) {
        return false;
    }
    
    if (strlen(g_config.wifi.password) < 8 || strlen(g_config.wifi.password) >= 64) {
        return false;
    }
    
    /* 验证OTA配置 */
    if (strlen(g_config.ota.host) == 0 || strlen(g_config.ota.host) >= 64) {
        return false;
    }
    
    if (g_config.ota.port == 0 || g_config.ota.port > 65535) {
        return false;
    }
    
    if (strlen(g_config.ota.path) == 0 || strlen(g_config.ota.path) >= 128) {
        return false;
    }
    
    /* 验证系统配置 */
    if (g_config.system.bootloader_delay_ms > 30000) {
        return false;
    }
    
    if (g_config.system.uart_baudrate < 9600 || g_config.system.uart_baudrate > 921600) {
        return false;
    }
    
    if (g_config.system.max_retry_count > 10) {
        return false;
    }
    
    return true;
}