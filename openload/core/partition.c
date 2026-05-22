/*
 * OpenLoad - 分区管理
 *
 * 分区表 ol_part_table() 由用户工程基于 partitions.def 实现 (X-macro 展开).
 * 框架在此处只负责查找、权限校验、转发到底层 flash device。
 */
#include "openload/partition.h"
#include "openload/ops/flash_ops.h"
#include "openload/crypto.h"
#include "openload/errno.h"
#include <string.h>
#include <stddef.h>

const ol_partition_t *ol_part_find(const char *name)
{
    if (!name) {
        return NULL;
    }
    uint32_t                count = 0;
    const ol_partition_t   *tbl   = ol_part_table(&count);
    for (uint32_t i = 0; i < count; ++i) {
        if (tbl[i].name && strcmp(tbl[i].name, name) == 0) {
            return &tbl[i];
        }
    }
    return NULL;
}

ol_flash_dev_t *ol_part_get_device(const ol_partition_t *p)
{
    return p ? ol_flash_dev_find(p->device_name) : NULL;
}

/* 范围 + 对齐检查辅助 */
static int check_range(const ol_partition_t *p, uint32_t off, uint32_t len)
{
    if (off > p->size || len > p->size - off) {
        return OL_E_PART_OUT_OF_RANGE;
    }
    return OL_OK;
}

int ol_part_read(const ol_partition_t *p, uint32_t off,
                 void *buf, uint32_t len)
{
    if (!p || !buf) { return OL_E_INVAL; }
    if (!(p->flags & OL_PART_FLAG_READABLE)) { return OL_E_PART_WRITE_DENIED; }
    int rc = check_range(p, off, len);
    if (rc != OL_OK) { return rc; }
    ol_flash_dev_t *d = ol_flash_dev_find(p->device_name);
    if (!d || !d->ops || !d->ops->read) { return OL_E_PART_NO_DEVICE; }
    return d->ops->read(d, p->offset + off, buf, len);
}

int ol_part_write(const ol_partition_t *p, uint32_t off,
                  const void *buf, uint32_t len)
{
    if (!p || !buf) { return OL_E_INVAL; }
    if (!(p->flags & OL_PART_FLAG_WRITABLE)) { return OL_E_PART_WRITE_DENIED; }
    int rc = check_range(p, off, len);
    if (rc != OL_OK) { return rc; }
    ol_flash_dev_t *d = ol_flash_dev_find(p->device_name);
    if (!d || !d->ops || !d->ops->write) { return OL_E_PART_NO_DEVICE; }
    /* 写入对齐由底层 device 检查; partition 不假设其粒度 */
    return d->ops->write(d, p->offset + off, buf, len);
}

int ol_part_erase(const ol_partition_t *p, uint32_t off, uint32_t len)
{
    if (!p) { return OL_E_INVAL; }
    if (!(p->flags & OL_PART_FLAG_WRITABLE)) { return OL_E_PART_WRITE_DENIED; }
    int rc = check_range(p, off, len);
    if (rc != OL_OK) { return rc; }
    ol_flash_dev_t *d = ol_flash_dev_find(p->device_name);
    if (!d || !d->ops || !d->ops->erase) { return OL_E_PART_NO_DEVICE; }
    /* 对齐到 sector_size 由底层 driver 自检, 避免框架与驱动重复校验 */
    return d->ops->erase(d, p->offset + off, len);
}

int ol_part_erase_all(const ol_partition_t *p)
{
    return p ? ol_part_erase(p, 0, p->size) : OL_E_INVAL;
}

int ol_part_verify_crc32(const ol_partition_t *p, uint32_t off,
                         uint32_t len, uint32_t expected)
{
    if (!p) { return OL_E_INVAL; }
    uint32_t got = ol_crc32_partition(p, off, len);
    return (got == expected) ? OL_OK : OL_E_IMAGE_PAYLOAD_CRC;
}
