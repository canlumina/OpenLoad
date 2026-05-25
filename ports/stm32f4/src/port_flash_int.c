/*
 * STM32F4 Port - 内部 Flash ops (STM32F407VGT6, 1MB)
 *
 * F4 Flash sector 不均匀:
 *   sector 0-3 (16K×4 = 64K)   ← bootloader 区
 *   sector 4   (64K)           ← 跳过不用 (跨大小不对齐, 简化分区)
 *   sector 5-11 (128K×7 = 896K) ← app / dual-bank / spare 区
 *
 * 设计取舍: dev.sector_size 暴露 16K (最小 sector), 让 ol_updater 用 16K 粒度
 * 对齐 erase_len; 本 driver 在 erase 内部按实际 sector lookup 智能批量擦,
 * 多擦的部分都在 caller 目标 partition 内 (前提: partition 边界 = sector 边界,
 * 由 partitions.def 保证). 这样既不改框架 API, 也支持 F4 非均匀布局.
 */
#include "openload/ops/flash_ops.h"
#include "openload/errno.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdint.h>

#define INT_FLASH_BASE          0x08000000u
#define INT_FLASH_SIZE          (1024u * 1024u)
#define INT_FLASH_MIN_SECTOR    (16u * 1024u)     /* 框架暴露的对齐粒度 */
#define INT_FLASH_WRITE_GRAN    4u                /* word program */

/* sector_id 用 ST FLASH_SECTOR_0..11. offset 是相对 INT_FLASH_BASE 的偏移. */
static const struct {
    uint32_t offset;
    uint32_t size;
    uint32_t sector_id;
} F4_SECTORS[] = {
    { 0x00000000u,  16u * 1024u, FLASH_SECTOR_0  },
    { 0x00004000u,  16u * 1024u, FLASH_SECTOR_1  },
    { 0x00008000u,  16u * 1024u, FLASH_SECTOR_2  },
    { 0x0000C000u,  16u * 1024u, FLASH_SECTOR_3  },
    { 0x00010000u,  64u * 1024u, FLASH_SECTOR_4  },
    { 0x00020000u, 128u * 1024u, FLASH_SECTOR_5  },
    { 0x00040000u, 128u * 1024u, FLASH_SECTOR_6  },
    { 0x00060000u, 128u * 1024u, FLASH_SECTOR_7  },
    { 0x00080000u, 128u * 1024u, FLASH_SECTOR_8  },
    { 0x000A0000u, 128u * 1024u, FLASH_SECTOR_9  },
    { 0x000C0000u, 128u * 1024u, FLASH_SECTOR_10 },
    { 0x000E0000u, 128u * 1024u, FLASH_SECTOR_11 },
};
#define F4_SECTOR_COUNT (sizeof(F4_SECTORS) / sizeof(F4_SECTORS[0]))

/* 找到 offset 所属的 sector index. -1 表示越界. */
static int find_sector(uint32_t offset)
{
    for (uint32_t i = 0; i < F4_SECTOR_COUNT; ++i) {
        if (offset >= F4_SECTORS[i].offset &&
            offset <  F4_SECTORS[i].offset + F4_SECTORS[i].size) {
            return (int)i;
        }
    }
    return -1;
}

static int int_flash_read(ol_flash_dev_t *dev, uint32_t offset,
                          void *buf, uint32_t len)
{
    (void)dev;
    memcpy(buf, (const void *)(INT_FLASH_BASE + offset), len);
    return OL_OK;
}

static int int_flash_unlock(ol_flash_dev_t *dev)
{
    (void)dev;
    return (HAL_FLASH_Unlock() == HAL_OK) ? OL_OK : OL_E_IO;
}

static int int_flash_lock(ol_flash_dev_t *dev)
{
    (void)dev;
    return (HAL_FLASH_Lock() == HAL_OK) ? OL_OK : OL_E_IO;
}

/* offset/len 必须按 MIN_SECTOR (16K) 对齐, 但实际擦的是整 sector. 例:
 *   erase(0x10000, 16K)  -> 擦 sector 4 (64K) 一整个
 *   erase(0x20000, 128K) -> 擦 sector 5 (128K)
 *   erase(0x20000, 256K) -> 擦 sector 5 + sector 6
 *
 * caller (ol_updater) 按 16K 粒度对齐 erase_len, 这里向后吞掉本 sector 剩余,
 * 跳到下一个 sector 头继续. 多擦的部分都落在 caller 目标 partition 内
 * (前提: partition 边界 = sector 边界, 由 partitions.def 保证). */
static int int_flash_erase(ol_flash_dev_t *dev, uint32_t offset, uint32_t len)
{
    (void)dev;
    if ((offset & (INT_FLASH_MIN_SECTOR - 1)) ||
        (len    & (INT_FLASH_MIN_SECTOR - 1))) {
        return OL_E_PART_ALIGN;
    }
    if (offset + len > INT_FLASH_SIZE) { return OL_E_PART_OUT_OF_RANGE; }
    if (HAL_FLASH_Unlock() != HAL_OK)  { return OL_E_IO; }

    uint32_t cur = offset;
    uint32_t end = offset + len;
    int rc = OL_OK;
    while (cur < end) {
        int idx = find_sector(cur);
        if (idx < 0) { rc = OL_E_PART_OUT_OF_RANGE; break; }

        FLASH_EraseInitTypeDef erase = {
            .TypeErase    = FLASH_TYPEERASE_SECTORS,
            .Banks        = FLASH_BANK_1,
            .Sector       = F4_SECTORS[idx].sector_id,
            .NbSectors    = 1,
            .VoltageRange = FLASH_VOLTAGE_RANGE_3,   /* 2.7-3.6V, 板上 3.3V */
        };
        uint32_t sector_err = 0xFFFFFFFFu;
        if (HAL_FLASHEx_Erase(&erase, &sector_err) != HAL_OK) {
            rc = OL_E_IO;
            break;
        }
        /* 跳到本 sector 末尾, 下一轮去下个 sector */
        cur = F4_SECTORS[idx].offset + F4_SECTORS[idx].size;
    }

    HAL_FLASH_Lock();
    return rc;
}

static int int_flash_write(ol_flash_dev_t *dev, uint32_t offset,
                           const void *buf, uint32_t len)
{
    (void)dev;
    if (offset & 0x3u) { return OL_E_PART_ALIGN; }
    if (HAL_FLASH_Unlock() != HAL_OK) { return OL_E_IO; }

    const uint8_t *p = (const uint8_t *)buf;
    uint32_t       addr = INT_FLASH_BASE + offset;
    uint32_t       done = 0;
    while (done < len) {
        uint32_t word = 0xFFFFFFFFu;
        uint32_t n    = (len - done >= 4) ? 4 : (len - done);
        memcpy(&word, p + done, n);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + done, word) != HAL_OK) {
            HAL_FLASH_Lock();
            return OL_E_IO;
        }
        done += 4;
    }
    HAL_FLASH_Lock();
    return OL_OK;
}

static const ol_flash_ops_t s_int_ops = {
    .read   = int_flash_read,
    .write  = int_flash_write,
    .erase  = int_flash_erase,
    .lock   = int_flash_lock,
    .unlock = int_flash_unlock,
};

static ol_flash_dev_t s_int_dev = {
    .name              = "internal",
    .base              = INT_FLASH_BASE,
    .size              = INT_FLASH_SIZE,
    .sector_size       = INT_FLASH_MIN_SECTOR,
    .write_granularity = INT_FLASH_WRITE_GRAN,
    .xip               = true,
    .ops               = &s_int_ops,
    .priv              = 0,
};

OL_FLASH_DEV_REGISTER(internal, &s_int_dev);
