#ifndef __DEV_FLASH_H_
#define __DEV_FLASH_H_

#include <stdint.h>

/* 分区信息结构 */
typedef struct {
    const char* name;
    uint32_t start_addr;
    uint32_t size;
    const char* description;
} flash_partition_t;

/* 外部Flash分区定义 */
typedef enum {
    FLASH_PARTITION_DOWNLOAD = 0,  // 下载区
    FLASH_PARTITION_BACKUP1,       // 备份区1
    FLASH_PARTITION_BACKUP2,       // 备份区2
    FLASH_PARTITION_BACKUP3,       // 备份区3
    FLASH_PARTITION_LOG,           // 日志区
    FLASH_PARTITION_CONFIG,        // 配置区
    FLASH_PARTITION_RESERVED,      // 预留区
    FLASH_PARTITION_MAX
} flash_partition_id_t;


/* 分区表 - 总容量8MB */
#define W25Q64_PARTITION_TABLE { \
    {"Download",  0x000000, 0x200000, "Firmware download area (2MB)"},     \
    {"Backup1",   0x200000, 0x070000, "Firmware backup slot 1 (448KB)"},   \
    {"Backup2",   0x270000, 0x070000, "Firmware backup slot 2 (448KB)"},   \
    {"Backup3",   0x2E0000, 0x070000, "Firmware backup slot 3 (448KB)"},   \
    {"Log",       0x350000, 0x080000, "System log area (512KB)"},          \
    {"Config",    0x3D0000, 0x010000, "Configuration area (64KB)"},        \
    {"Reserved",  0x3E0000, 0x420000, "Reserved area (4.125MB)"}           \
}


#endif
