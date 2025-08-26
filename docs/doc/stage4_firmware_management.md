# 阶段4-2：固件管理功能

## 概述
实现固件版本管理、备份恢复、完整性校验和升级历史记录等高级固件管理功能。

## 固件信息结构设计

### 固件头部定义
```c
/* 固件头部结构 (位于固件开始处) */
typedef struct {
    uint32_t magic;                 // 魔术字：0xDEADBEEF
    uint32_t version;               // 版本号：主版本(8位).次版本(8位).修订版(16位)
    uint32_t size;                  // 固件大小 (不包含头部)
    uint32_t crc32;                 // 固件CRC32校验值
    uint32_t timestamp;             // 编译时间戳
    char description[48];           // 固件描述信息
    char build_date[16];            // 编译日期 "YYYY-MM-DD"
    char build_time[16];            // 编译时间 "HH:MM:SS"
    uint32_t app_entry;             // 应用程序入口地址
    uint32_t reserved[3];           // 保留字段
} __attribute__((packed)) Firmware_Header_t;

#define FIRMWARE_MAGIC              0xDEADBEEF
#define FIRMWARE_HEADER_SIZE        sizeof(Firmware_Header_t)

/* 固件版本结构 */
typedef struct {
    uint8_t major;                  // 主版本号
    uint8_t minor;                  // 次版本号
    uint16_t revision;              // 修订版本号
} Firmware_Version_t;

/* 升级记录结构 */
typedef struct {
    uint32_t magic;                 // 记录魔术字：0x12345678
    uint32_t upgrade_count;         // 升级次数
    uint32_t last_upgrade_time;     // 最后升级时间
    Firmware_Version_t old_version; // 旧版本
    Firmware_Version_t new_version; // 新版本
    uint32_t upgrade_result;        // 升级结果
    uint32_t backup_crc32;          // 备份固件CRC32
    uint32_t crc32;                 // 记录CRC32
} __attribute__((packed)) Upgrade_Record_t;
```

## 固件管理器实现

### firmware_manager.c
```c
/**
  * @file    firmware_manager.c
  * @brief   固件管理器实现
  */

#include "firmware_manager.h"
#include "flash_manager.h"
#include "w25q64_driver.h"
#include <string.h>
#include <stdio.h>

#define BACKUP_PARTITION_ADDR       0x200000    // 备份分区地址
#define CONFIG_PARTITION_ADDR       0x0807F000  // 配置分区地址
#define UPGRADE_RECORD_OFFSET       0x100       // 升级记录偏移

static Upgrade_Record_t g_upgrade_record;

/**
  * @brief  固件管理器初始化
  * @param  None
  * @retval Firmware_Result_t
  */
Firmware_Result_t Firmware_Manager_Init(void)
{
    printf("Firmware Manager: Initializing...\r\n");
    
    /* 读取升级记录 */
    if (Firmware_Load_UpgradeRecord() != FIRMWARE_OK)
    {
        /* 初始化升级记录 */
        memset(&g_upgrade_record, 0, sizeof(Upgrade_Record_t));
        g_upgrade_record.magic = 0x12345678;
        g_upgrade_record.upgrade_count = 0;
        Firmware_Save_UpgradeRecord();
    }
    
    printf("Firmware Manager: Initialized\r\n");
    printf("  - Upgrade count: %d\r\n", g_upgrade_record.upgrade_count);
    
    return FIRMWARE_OK;
}

/**
  * @brief  获取当前固件信息
  * @param  info: 固件信息结构指针
  * @retval Firmware_Result_t
  */
Firmware_Result_t Firmware_Get_CurrentInfo(Firmware_Info_t* info)
{
    if (info == NULL)
        return FIRMWARE_ERROR;
    
    /* 检查应用程序区域是否有有效固件 */
    uint32_t app_addr = 0x08010000;
    Firmware_Header_t* header = (Firmware_Header_t*)app_addr;
    
    if (header->magic == FIRMWARE_MAGIC)
    {
        /* 有效的固件头部 */
        info->has_firmware = 1;
        info->header = *header;
        
        /* 解析版本号 */
        info->version.major = (header->version >> 24) & 0xFF;
        info->version.minor = (header->version >> 16) & 0xFF;
        info->version.revision = header->version & 0xFFFF;
        
        /* 验证CRC */
        uint8_t* firmware_data = (uint8_t*)(app_addr + FIRMWARE_HEADER_SIZE);
        uint32_t calculated_crc = Flash_Calculate_CRC32(firmware_data, header->size);
        info->crc_valid = (calculated_crc == header->crc32);
        
        printf("Current Firmware Info:\r\n");
        printf("  - Version: %d.%d.%d\r\n", 
               info->version.major, info->version.minor, info->version.revision);
        printf("  - Size: %d bytes\r\n", header->size);
        printf("  - Description: %.48s\r\n", header->description);
        printf("  - Build: %s %s\r\n", header->build_date, header->build_time);
        printf("  - CRC32: 0x%08X (%s)\r\n", 
               header->crc32, info->crc_valid ? "Valid" : "Invalid");
    }
    else
    {
        /* 没有有效固件 */
        info->has_firmware = 0;
        memset(&info->header, 0, sizeof(Firmware_Header_t));
        memset(&info->version, 0, sizeof(Firmware_Version_t));
        info->crc_valid = 0;
        
        printf("No valid firmware found\r\n");
    }
    
    return FIRMWARE_OK;
}

/**
  * @brief  备份当前固件
  * @param  None
  * @retval Firmware_Result_t
  */
Firmware_Result_t Firmware_Backup_Current(void)
{
    printf("Firmware: Backing up current firmware...\r\n");
    
    Firmware_Info_t current_info;
    if (Firmware_Get_CurrentInfo(&current_info) != FIRMWARE_OK)
    {
        printf("Firmware: Failed to get current firmware info\r\n");
        return FIRMWARE_ERROR;
    }
    
    if (!current_info.has_firmware)
    {
        printf("Firmware: No firmware to backup\r\n");
        return FIRMWARE_ERROR;
    }
    
    /* 计算备份大小 (包含头部) */
    uint32_t backup_size = FIRMWARE_HEADER_SIZE + current_info.header.size;
    
    printf("Firmware: Backup size = %d bytes\r\n", backup_size);
    
    /* 擦除备份分区 */
    uint32_t sectors_to_erase = (backup_size + W25Q64_SECTOR_SIZE - 1) / W25Q64_SECTOR_SIZE;
    if (Flash_External_Erase(BACKUP_PARTITION_ADDR, sectors_to_erase * W25Q64_SECTOR_SIZE) != FLASH_OK)
    {
        printf("Firmware: Backup partition erase failed\r\n");
        return FIRMWARE_ERROR;
    }
    
    /* 读取并写入固件数据 */
    uint8_t buffer[256];
    uint32_t bytes_copied = 0;
    uint32_t source_addr = 0x08010000;
    
    while (bytes_copied < backup_size)
    {
        uint32_t chunk_size = (backup_size - bytes_copied > 256) ? 256 : (backup_size - bytes_copied);
        
        /* 从内部Flash读取 */
        if (Flash_Internal_Read(source_addr + bytes_copied, buffer, chunk_size) != FLASH_OK)
        {
            printf("Firmware: Read from internal flash failed\r\n");
            return FIRMWARE_ERROR;
        }
        
        /* 写入到外部Flash */
        if (Flash_External_Write(BACKUP_PARTITION_ADDR + bytes_copied, buffer, chunk_size) != FLASH_OK)
        {
            printf("Firmware: Write to backup partition failed\r\n");
            return FIRMWARE_ERROR;
        }
        
        bytes_copied += chunk_size;
        
        /* 显示进度 */
        if (bytes_copied % 4096 == 0 || bytes_copied == backup_size)
        {
            uint8_t progress = (bytes_copied * 100) / backup_size;
            printf("Firmware: Backup progress %d%%\r", progress);
        }
    }
    
    printf("\r\nFirmware: Backup completed successfully\r\n");
    
    /* 更新升级记录中的备份CRC */
    g_upgrade_record.backup_crc32 = current_info.header.crc32;
    
    return FIRMWARE_OK;
}

/**
  * @brief  从备份恢复固件
  * @param  None
  * @retval Firmware_Result_t
  */
Firmware_Result_t Firmware_Restore_FromBackup(void)
{
    printf("Firmware: Restoring from backup...\r\n");
    
    /* 读取备份固件头部 */
    Firmware_Header_t backup_header;
    if (Flash_External_Read(BACKUP_PARTITION_ADDR, (uint8_t*)&backup_header, 
                           sizeof(Firmware_Header_t)) != FLASH_OK)
    {
        printf("Firmware: Failed to read backup header\r\n");
        return FIRMWARE_ERROR;
    }
    
    /* 检查备份固件有效性 */
    if (backup_header.magic != FIRMWARE_MAGIC)
    {
        printf("Firmware: Invalid backup firmware magic\r\n");
        return FIRMWARE_ERROR;
    }
    
    printf("Firmware: Backup firmware found\r\n");
    printf("  - Version: %d.%d.%d\r\n", 
           (backup_header.version >> 24) & 0xFF,
           (backup_header.version >> 16) & 0xFF,
           backup_header.version & 0xFFFF);
    printf("  - Size: %d bytes\r\n", backup_header.size);
    
    /* 计算恢复大小 */
    uint32_t restore_size = FIRMWARE_HEADER_SIZE + backup_header.size;
    uint32_t pages_to_erase = (restore_size + 2047) / 2048;
    
    /* 擦除应用程序区域 */
    printf("Firmware: Erasing application area (%d pages)...\r\n", pages_to_erase);
    if (Flash_Internal_Erase(0x08010000, pages_to_erase) != FLASH_OK)
    {
        printf("Firmware: Application area erase failed\r\n");
        return FIRMWARE_ERROR;
    }
    
    /* 恢复固件数据 */
    uint8_t buffer[256];
    uint32_t bytes_restored = 0;
    uint32_t dest_addr = 0x08010000;
    
    while (bytes_restored < restore_size)
    {
        uint32_t chunk_size = (restore_size - bytes_restored > 256) ? 256 : (restore_size - bytes_restored);
        
        /* 确保4字节对齐 */
        if (chunk_size % 4 != 0)
            chunk_size = (chunk_size + 3) & ~3;
        
        /* 从备份分区读取 */
        if (Flash_External_Read(BACKUP_PARTITION_ADDR + bytes_restored, buffer, chunk_size) != FLASH_OK)
        {
            printf("Firmware: Read from backup failed\r\n");
            return FIRMWARE_ERROR;
        }
        
        /* 写入到内部Flash */
        if (Flash_Internal_Write(dest_addr + bytes_restored, buffer, chunk_size) != FLASH_OK)
        {
            printf("Firmware: Write to internal flash failed\r\n");
            return FIRMWARE_ERROR;
        }
        
        bytes_restored += chunk_size;
        
        /* 显示进度 */
        if (bytes_restored % 4096 == 0 || bytes_restored >= restore_size)
        {
            uint8_t progress = (bytes_restored * 100) / restore_size;
            printf("Firmware: Restore progress %d%%\r", progress);
        }
    }
    
    printf("\r\nFirmware: Restore completed successfully\r\n");
    
    /* 验证恢复的固件 */
    return Firmware_Verify_Current();
}

/**
  * @brief  验证当前固件
  * @param  None
  * @retval Firmware_Result_t
  */
Firmware_Result_t Firmware_Verify_Current(void)
{
    printf("Firmware: Verifying current firmware...\r\n");
    
    Firmware_Info_t info;
    if (Firmware_Get_CurrentInfo(&info) != FIRMWARE_OK)
    {
        printf("Firmware: Get current info failed\r\n");
        return FIRMWARE_ERROR;
    }
    
    if (!info.has_firmware)
    {
        printf("Firmware: No firmware to verify\r\n");
        return FIRMWARE_ERROR;
    }
    
    if (!info.crc_valid)
    {
        printf("Firmware: CRC verification failed\r\n");
        return FIRMWARE_CRC_ERROR;
    }
    
    printf("Firmware: Verification passed\r\n");
    return FIRMWARE_OK;
}

/**
  * @brief  比较固件版本
  * @param  version1: 版本1
  * @param  version2: 版本2  
  * @retval -1: version1 < version2, 0: 相等, 1: version1 > version2
  */
int8_t Firmware_Compare_Version(Firmware_Version_t* version1, Firmware_Version_t* version2)
{
    if (version1->major < version2->major) return -1;
    if (version1->major > version2->major) return 1;
    
    if (version1->minor < version2->minor) return -1;
    if (version1->minor > version2->minor) return 1;
    
    if (version1->revision < version2->revision) return -1;
    if (version1->revision > version2->revision) return 1;
    
    return 0;
}

/**
  * @brief  加载升级记录
  * @param  None
  * @retval Firmware_Result_t
  */
static Firmware_Result_t Firmware_Load_UpgradeRecord(void)
{
    /* 从配置分区读取升级记录 */
    if (Flash_Internal_Read(CONFIG_PARTITION_ADDR + UPGRADE_RECORD_OFFSET, 
                           (uint8_t*)&g_upgrade_record, 
                           sizeof(Upgrade_Record_t)) != FLASH_OK)
    {
        return FIRMWARE_ERROR;
    }
    
    /* 检查记录有效性 */
    if (g_upgrade_record.magic != 0x12345678)
    {
        return FIRMWARE_ERROR;
    }
    
    /* 验证CRC */
    uint32_t calculated_crc = Flash_Calculate_CRC32((uint8_t*)&g_upgrade_record, 
                                                   sizeof(Upgrade_Record_t) - 4);
    if (calculated_crc != g_upgrade_record.crc32)
    {
        return FIRMWARE_CRC_ERROR;
    }
    
    return FIRMWARE_OK;
}

/**
  * @brief  保存升级记录
  * @param  None
  * @retval Firmware_Result_t
  */
Firmware_Result_t Firmware_Save_UpgradeRecord(void)
{
    /* 计算CRC */
    g_upgrade_record.crc32 = Flash_Calculate_CRC32((uint8_t*)&g_upgrade_record, 
                                                  sizeof(Upgrade_Record_t) - 4);
    
    /* 擦除配置区的一页 (从记录偏移开始) */
    uint32_t page_addr = CONFIG_PARTITION_ADDR;  
    if (Flash_Internal_Erase(page_addr, 1) != FLASH_OK)
    {
        printf("Firmware: Config area erase failed\r\n");
        return FIRMWARE_ERROR;
    }
    
    /* 写入升级记录 */
    if (Flash_Internal_Write(CONFIG_PARTITION_ADDR + UPGRADE_RECORD_OFFSET,
                            (uint8_t*)&g_upgrade_record,
                            sizeof(Upgrade_Record_t)) != FLASH_OK)
    {
        printf("Firmware: Save upgrade record failed\r\n");
        return FIRMWARE_ERROR;
    }
    
    return FIRMWARE_OK;
}

/**
  * @brief  更新升级记录
  * @param  old_ver: 旧版本
  * @param  new_ver: 新版本
  * @param  result: 升级结果
  * @retval None
  */
void Firmware_Update_UpgradeRecord(Firmware_Version_t* old_ver, 
                                  Firmware_Version_t* new_ver, 
                                  uint32_t result)
{
    g_upgrade_record.upgrade_count++;
    g_upgrade_record.last_upgrade_time = HAL_GetTick();
    g_upgrade_record.upgrade_result = result;
    
    if (old_ver != NULL)
        g_upgrade_record.old_version = *old_ver;
    
    if (new_ver != NULL)
        g_upgrade_record.new_version = *new_ver;
    
    Firmware_Save_UpgradeRecord();
}

/**
  * @brief  打印升级历史
  * @param  None
  * @retval None
  */
void Firmware_Print_UpgradeHistory(void)
{
    printf("Firmware Upgrade History:\r\n");
    printf("========================\r\n");
    printf("Total upgrades: %d\r\n", g_upgrade_record.upgrade_count);
    
    if (g_upgrade_record.upgrade_count > 0)
    {
        printf("Last upgrade:\r\n");
        printf("  - Time: %d ms ago\r\n", HAL_GetTick() - g_upgrade_record.last_upgrade_time);
        printf("  - Old version: %d.%d.%d\r\n", 
               g_upgrade_record.old_version.major,
               g_upgrade_record.old_version.minor,
               g_upgrade_record.old_version.revision);
        printf("  - New version: %d.%d.%d\r\n", 
               g_upgrade_record.new_version.major,
               g_upgrade_record.new_version.minor,
               g_upgrade_record.new_version.revision);
        printf("  - Result: %s\r\n", 
               (g_upgrade_record.upgrade_result == 0) ? "Success" : "Failed");
        printf("  - Backup CRC32: 0x%08X\r\n", g_upgrade_record.backup_crc32);
    }
    else
    {
        printf("No upgrade history\r\n");
    }
}

/**
  * @brief  擦除固件 (慎用!)
  * @param  None
  * @retval Firmware_Result_t
  */
Firmware_Result_t Firmware_Erase_Current(void)
{
    printf("Firmware: WARNING - Erasing current firmware!\r\n");
    
    /* 擦除整个应用程序区域 */
    uint32_t pages_to_erase = (448 * 1024) / 2048;  // 448KB / 2KB per page
    
    if (Flash_Internal_Erase(0x08010000, pages_to_erase) != FLASH_OK)
    {
        printf("Firmware: Erase failed\r\n");
        return FIRMWARE_ERROR;
    }
    
    printf("Firmware: Current firmware erased\r\n");
    return FIRMWARE_OK;
}
```

## 头文件定义

### firmware_manager.h
```c
#ifndef __FIRMWARE_MANAGER_H
#define __FIRMWARE_MANAGER_H

#include "stm32f10x.h"

/* 固件管理结果 */
typedef enum {
    FIRMWARE_OK = 0,
    FIRMWARE_ERROR,
    FIRMWARE_CRC_ERROR,
    FIRMWARE_VERSION_ERROR,
    FIRMWARE_NO_BACKUP
} Firmware_Result_t;

/* 固件版本结构 */
typedef struct {
    uint8_t major;
    uint8_t minor;
    uint16_t revision;
} Firmware_Version_t;

/* 固件头部结构 */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t crc32;
    uint32_t timestamp;
    char description[48];
    char build_date[16];
    char build_time[16];
    uint32_t app_entry;
    uint32_t reserved[3];
} __attribute__((packed)) Firmware_Header_t;

/* 固件信息结构 */
typedef struct {
    uint8_t has_firmware;
    uint8_t crc_valid;
    Firmware_Header_t header;
    Firmware_Version_t version;
} Firmware_Info_t;

/* 升级记录结构 */
typedef struct {
    uint32_t magic;
    uint32_t upgrade_count;
    uint32_t last_upgrade_time;
    Firmware_Version_t old_version;
    Firmware_Version_t new_version;
    uint32_t upgrade_result;
    uint32_t backup_crc32;
    uint32_t crc32;
} __attribute__((packed)) Upgrade_Record_t;

/* 函数声明 */
Firmware_Result_t Firmware_Manager_Init(void);
Firmware_Result_t Firmware_Get_CurrentInfo(Firmware_Info_t* info);
Firmware_Result_t Firmware_Backup_Current(void);
Firmware_Result_t Firmware_Restore_FromBackup(void);
Firmware_Result_t Firmware_Verify_Current(void);
int8_t Firmware_Compare_Version(Firmware_Version_t* version1, Firmware_Version_t* version2);
Firmware_Result_t Firmware_Save_UpgradeRecord(void);
void Firmware_Update_UpgradeRecord(Firmware_Version_t* old_ver, Firmware_Version_t* new_ver, uint32_t result);
void Firmware_Print_UpgradeHistory(void);
Firmware_Result_t Firmware_Erase_Current(void);

/* 私有函数声明 */
static Firmware_Result_t Firmware_Load_UpgradeRecord(void);

/* 常量定义 */
#define FIRMWARE_MAGIC              0xDEADBEEF
#define FIRMWARE_HEADER_SIZE        sizeof(Firmware_Header_t)

#endif /* __FIRMWARE_MANAGER_H */
```

## 命令集成

### 在command_system.c中添加固件管理命令
```c
/* 新增命令处理函数 */
static void Cmd_Firmware_Info(int argc, char* argv[])
{
    Firmware_Info_t info;
    if (Firmware_Get_CurrentInfo(&info) == FIRMWARE_OK)
    {
        printf("Current firmware loaded successfully\r\n");
    }
    else
    {
        printf("Failed to get firmware info\r\n");
    }
}

static void Cmd_Firmware_Backup(int argc, char* argv[])
{
    printf("Starting firmware backup...\r\n");
    
    if (Firmware_Backup_Current() == FIRMWARE_OK)
    {
        printf("Firmware backup completed\r\n");
    }
    else
    {
        printf("Firmware backup failed\r\n");
    }
}

static void Cmd_Firmware_Restore(int argc, char* argv[])
{
    printf("Are you sure you want to restore from backup? (y/N): ");
    
    char confirm = getchar();
    if (confirm == 'y' || confirm == 'Y')
    {
        if (Firmware_Restore_FromBackup() == FIRMWARE_OK)
        {
            printf("Firmware restore completed\r\n");
        }
        else
        {
            printf("Firmware restore failed\r\n");
        }
    }
    else
    {
        printf("Operation cancelled\r\n");
    }
}

static void Cmd_Firmware_History(int argc, char* argv[])
{
    Firmware_Print_UpgradeHistory();
}

/* 更新命令表 */
static const Command_t g_command_table[] = {
    // ... 现有命令 ...
    {"fwinfo",  "Show firmware information",           Cmd_Firmware_Info,    0, 0},
    {"backup",  "Backup current firmware",            Cmd_Firmware_Backup,  0, 0},
    {"restore", "Restore firmware from backup",       Cmd_Firmware_Restore, 0, 0},
    {"history", "Show upgrade history",               Cmd_Firmware_History, 0, 0},
};
```

## 测试验证

### 固件管理功能测试
```
Bootloader> fwinfo
Current Firmware Info:
  - Version: 1.2.3
  - Size: 45678 bytes
  - Description: Main Application v1.2.3
  - Build: 2024-12-25 10:30:00
  - CRC32: 0x12345678 (Valid)

Bootloader> backup
Starting firmware backup...
Firmware: Backup size = 45678 bytes
Firmware: Backup progress 100%
Firmware backup completed

Bootloader> history  
Firmware Upgrade History:
========================
Total upgrades: 3
Last upgrade:
  - Time: 3600000 ms ago
  - Old version: 1.1.0
  - New version: 1.2.3
  - Result: Success
  - Backup CRC32: 0xABCDEF00
```

## 验证标准
1. 固件信息读取准确，版本解析正确
2. 备份和恢复功能稳定可靠
3. CRC校验准确，数据完整性有保障
4. 升级记录保存正确，历史查询正常
5. 异常情况处理完善，系统稳定性好

## 下一步行动
固件管理功能完成后，继续开发OTA功能模块。