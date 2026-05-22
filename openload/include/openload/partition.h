/*
 * OpenLoad - 分区管理
 *
 * 分区表在用户工程的 partitions.def 中用 OL_PART(...) 宏声明,
 * 框架编译期展开为静态数组。每个分区引用一个已注册的 flash device。
 */
#pragma once

#include <stdint.h>
#include "openload/ops/flash_ops.h"

/* 分区权限标志 (位或组合) */
#define OL_PART_FLAG_READABLE   (1u << 0)
#define OL_PART_FLAG_WRITABLE   (1u << 1)
#define OL_PART_FLAG_EXECUTABLE (1u << 2)  /* App 区: 可由 jump 跳入 */
#define OL_PART_FLAG_ENCRYPTED  (1u << 3)  /* 区内固件预期带加密标记 */
#define OL_PART_FLAG_SIGNED     (1u << 4)  /* 区内固件预期带签名 */

typedef struct {
    const char *name;
    const char *device_name;   /* 引用的 flash_dev 名字 (例如 "internal") */
    uint32_t    offset;        /* 设备内偏移 */
    uint32_t    size;
    uint32_t    flags;
} ol_partition_t;

/**
 * @brief 按名字查找分区. 未找到返回 NULL.
 */
const ol_partition_t *ol_part_find(const char *name);

/**
 * @brief 解析分区底层 flash device (供 updater/receiver 直接获取设备粒度信息).
 */
ol_flash_dev_t *ol_part_get_device(const ol_partition_t *p);

/**
 * @brief 分区内读. 内部转发到底层 flash device.
 * @param  off  分区内偏移
 * @return OL_OK / 负数错误码.
 */
int ol_part_read(const ol_partition_t *p, uint32_t off,
                 void *buf, uint32_t len);

/**
 * @brief 分区内写. 会拒绝写入未授 WRITABLE 标志的分区.
 *        调用方必须确保目标 sector 已擦除。
 */
int ol_part_write(const ol_partition_t *p, uint32_t off,
                  const void *buf, uint32_t len);

/**
 * @brief 分区内擦除. off + len 必须按设备 sector_size 对齐.
 */
int ol_part_erase(const ol_partition_t *p, uint32_t off, uint32_t len);

/** 擦除整个分区. */
int ol_part_erase_all(const ol_partition_t *p);

/**
 * @brief 计算分区指定范围的 CRC32 并与期望值对比.
 * @return OL_OK = 匹配, OL_E_IMAGE_PAYLOAD_CRC = 不匹配, 其他 = IO 错误.
 */
int ol_part_verify_crc32(const ol_partition_t *p, uint32_t off,
                         uint32_t len, uint32_t expected);

/* 框架内部: 由 partitions.def 展开得到全部分区. */
const ol_partition_t *ol_part_table(uint32_t *count_out);
