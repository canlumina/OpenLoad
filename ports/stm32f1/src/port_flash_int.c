/*
 * STM32F1 Port - 内部 Flash ops
 *
 * STM32F103ZET6: 512KB Flash, page size = 2KB, 编程粒度 = 16-bit halfword
 * (本实现按 32-bit word 编程, HAL_FLASH_Program 内部分两次写)。
 *
 * 内部 Flash 是 XIP 设备, read 直接 memcpy。
 */
#include "openload/ops/flash_ops.h"
#include "openload/errno.h"
#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdint.h>

#define INT_FLASH_BASE          0x08000000u
#define INT_FLASH_SIZE          (512u * 1024u)
#define INT_FLASH_SECTOR_SIZE   (2u * 1024u)
#define INT_FLASH_WRITE_GRAN    4u

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

static int int_flash_erase(ol_flash_dev_t *dev, uint32_t offset, uint32_t len)
{
    (void)dev;
    if ((offset & (INT_FLASH_SECTOR_SIZE - 1)) ||
        (len    & (INT_FLASH_SECTOR_SIZE - 1))) {
        return OL_E_PART_ALIGN;
    }
    if (HAL_FLASH_Unlock() != HAL_OK) { return OL_E_IO; }

    FLASH_EraseInitTypeDef erase = {
        .TypeErase   = FLASH_TYPEERASE_PAGES,
        .Banks       = FLASH_BANK_1,
        .PageAddress = INT_FLASH_BASE + offset,
        .NbPages     = len / INT_FLASH_SECTOR_SIZE,
    };
    uint32_t page_error = 0;
    HAL_StatusTypeDef st = HAL_FLASHEx_Erase(&erase, &page_error);
    HAL_FLASH_Lock();
    return (st == HAL_OK) ? OL_OK : OL_E_IO;
}

static int int_flash_write(ol_flash_dev_t *dev, uint32_t offset,
                           const void *buf, uint32_t len)
{
    (void)dev;
    /* 写入对齐: HAL_FLASH_Program WORD 要求按 4 字节对齐. 尾部不足时补 0xFF. */
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
    .sector_size       = INT_FLASH_SECTOR_SIZE,
    .write_granularity = INT_FLASH_WRITE_GRAN,
    .xip               = true,
    .ops               = &s_int_ops,
    .priv              = 0,
};

OL_FLASH_DEV_REGISTER(internal, &s_int_dev);
