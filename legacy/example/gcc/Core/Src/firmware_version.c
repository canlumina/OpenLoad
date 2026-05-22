#include "firmware_version.h"
#include "main.h"
#include "dev_usart.h"
#include "bootloader_cmd.h"
#include <string.h>
#include <stdio.h>

/* print函数的前向声明，具体实现在bootloader_cmd.c中 */
void print_str(const char* str);
void print_hex(uint32_t val);
void print_dec(uint32_t val);

/**
 * @brief 获取指定地址固件的版本信息
 */
bool firmware_version_get_info(uint32_t firmware_addr, firmware_info_t* info)
{
    if (!info) return false;
    
    /* 检查固件起始地址的合法性 */
    if (firmware_addr < 0x08000000 || firmware_addr > 0x08080000) {
        return false;
    }
    
    /* 读取版本信息结构 */
    firmware_info_t* flash_info = (firmware_info_t*)(firmware_addr + FIRMWARE_INFO_OFFSET);
    memcpy(info, flash_info, sizeof(firmware_info_t));
    
    /* 验证魔数 */
    return firmware_version_validate_info(info);
}

/**
 * @brief 获取当前运行固件的版本信息
 */
bool firmware_version_get_current(firmware_info_t* info)
{
    return firmware_version_get_info(APP_START_ADDR, info);
}

/**
 * @brief 比较两个版本
 */
int firmware_version_compare(const firmware_version_t* v1, const firmware_version_t* v2)
{
    if (!v1 || !v2) return 0;
    
    /* 按优先级比较：major -> minor -> patch -> build */
    if (v1->major != v2->major) {
        return (v1->major > v2->major) ? 1 : -1;
    }
    
    if (v1->minor != v2->minor) {
        return (v1->minor > v2->minor) ? 1 : -1;
    }
    
    if (v1->patch != v2->patch) {
        return (v1->patch > v2->patch) ? 1 : -1;
    }
    
    if (v1->build != v2->build) {
        return (v1->build > v2->build) ? 1 : -1;
    }
    
    return 0; /* 版本相同 */
}

/**
 * @brief 版本号转换为字符串
 */
int firmware_version_to_string(const firmware_version_t* version, char* buffer, uint32_t buffer_size)
{
    if (!version || !buffer || buffer_size < 16) return 0;
    
    return snprintf(buffer, buffer_size, "v%u.%u.%u.%u", 
                   version->major, version->minor, version->patch, version->build);
}

/**
 * @brief 显示版本信息 - 实现在bootloader_cmd.c中
 */
void firmware_version_print_info(const firmware_info_t* info)
{
    /* 实际的打印实现在bootloader_cmd.c中，因为print函数在那里定义 */
    (void)info;
}

/**
 * @brief 验证固件信息结构的有效性
 */
bool firmware_version_validate_info(const firmware_info_t* info)
{
    if (!info) return false;
    
    /* 检查魔数 */
    if (info->magic != FIRMWARE_INFO_MAGIC) {
        return false;
    }
    
    /* 检查版本号的合理性 */
    if (info->version.major > 999 || 
        info->version.minor > 999 || 
        info->version.patch > 999 ||
        info->version.build > 99999) {
        return false;
    }
    
    /* 检查字符串是否以null结尾 */
    if (info->build_date[11] != '\0' || 
        info->build_time[9] != '\0' ||
        info->version_string[15] != '\0') {
        return false;
    }
    
    return true;
}

/**
 * @brief 创建当前固件的版本信息
 * 注：这个函数通常在链接时通过脚本生成，这里提供一个示例实现
 */
void firmware_version_create_current_info(firmware_info_t* info)
{
    if (!info) return;
    
    memset(info, 0, sizeof(firmware_info_t));
    
    info->magic = FIRMWARE_INFO_MAGIC;
    info->version.major = FW_VERSION_MAJOR;
    info->version.minor = FW_VERSION_MINOR;
    info->version.patch = FW_VERSION_PATCH;
    info->version.build = FW_VERSION_BUILD;
    
    /* 设置构建时间戳（编译时间） */
    info->build_timestamp = 0; /* TODO: 在构建时通过脚本设置 */
    
    /* 构建日期和时间字符串 */
    snprintf(info->build_date, sizeof(info->build_date), "%s", __DATE__);
    snprintf(info->build_time, sizeof(info->build_time), "%s", __TIME__);
    
    /* 版本字符串 */
    firmware_version_to_string(&info->version, info->version_string, sizeof(info->version_string));
}