#ifndef FIRMWARE_VERSION_H
#define FIRMWARE_VERSION_H

#include <stdint.h>
#include <stdbool.h>

/* 版本号结构定义 */
typedef struct {
    uint16_t major;          /* 主版本号：重大功能更新、API变更 */
    uint16_t minor;          /* 次版本号：功能增加、改进 */
    uint16_t patch;          /* 补丁版本号：bug修复、小改动 */
    uint16_t build;          /* 构建号：每次构建递增 */
} firmware_version_t;

/* 固件信息结构（放置在固件开始处的固定位置） */
typedef struct {
    uint32_t magic;                    /* 魔数：0x46574556 ("FWEV") */
    firmware_version_t version;        /* 版本信息 */
    uint32_t build_timestamp;          /* 构建时间戳 */
    char build_date[12];               /* 构建日期字符串 "YYYY-MM-DD" + \0 */
    char build_time[10];               /* 构建时间字符串 "HH:MM:SS" + \0 */
    char version_string[16];           /* 版本字符串 "v1.2.3.1234" + \0 */
    uint8_t reserved[14];              /* 保留字段，总共64字节 */
} __attribute__((packed)) firmware_info_t;

/* 版本管理常量 */
#define FIRMWARE_INFO_MAGIC     0x46574556  /* "FWEV" */
#define FIRMWARE_INFO_OFFSET    0x200       /* 固件信息在Flash中的偏移（512字节后） */

/* 当前固件版本定义（编译时设置） */
#ifndef FW_VERSION_MAJOR
#define FW_VERSION_MAJOR        1
#endif

#ifndef FW_VERSION_MINOR
#define FW_VERSION_MINOR        0
#endif

#ifndef FW_VERSION_PATCH
#define FW_VERSION_PATCH        0
#endif

#ifndef FW_VERSION_BUILD
#define FW_VERSION_BUILD        1
#endif

/* 函数声明 */

/**
 * @brief 获取当前运行固件的版本信息
 * @param info 输出的固件信息结构
 * @return true=成功，false=未找到版本信息
 */
bool firmware_version_get_current(firmware_info_t* info);

/**
 * @brief 获取指定地址固件的版本信息
 * @param firmware_addr 固件起始地址
 * @param info 输出的固件信息结构
 * @return true=成功，false=未找到版本信息
 */
bool firmware_version_get_info(uint32_t firmware_addr, firmware_info_t* info);

/**
 * @brief 比较两个版本
 * @param v1 版本1
 * @param v2 版本2
 * @return >0: v1>v2, =0: v1==v2, <0: v1<v2
 */
int firmware_version_compare(const firmware_version_t* v1, const firmware_version_t* v2);

/**
 * @brief 版本号转换为字符串
 * @param version 版本结构
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 格式化后的字符串长度
 */
int firmware_version_to_string(const firmware_version_t* version, char* buffer, uint32_t buffer_size);

/**
 * @brief 显示版本信息
 * @param info 固件信息结构
 */
void firmware_version_print_info(const firmware_info_t* info);

/**
 * @brief 验证固件信息结构的有效性
 * @param info 固件信息结构
 * @return true=有效，false=无效
 */
bool firmware_version_validate_info(const firmware_info_t* info);

#endif /* FIRMWARE_VERSION_H */