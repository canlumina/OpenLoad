/*
 * OpenLoad - 升级编排
 *
 * 当前 M1 仅实现 STAGING 策略: 通过 receiver 把固件下载到 staging 分区,
 * 校验头 + payload CRC, 然后流式拷贝到 target 分区, 再次校验。
 *
 * BACKUP / DUAL_BANK 策略留接口, M2/M3 落地。
 */
#include "openload/updater.h"
#include "openload/receiver.h"
#include "openload/partition.h"
#include "openload/image.h"
#include "openload/logger.h"
#include "openload/errno.h"
#include "openload/config.h"
#include "openload/ops/io_ops.h"
#include "openload/ops/flash_ops.h"
#include "openload/ops/sys_ops.h"
#include <string.h>
#include <stddef.h>

extern ol_receiver_t * const __ol_receivers_start[];
extern ol_receiver_t * const __ol_receivers_end[];

ol_receiver_t *ol_receiver_find(const char *name)
{
    if (!name) { return NULL; }
    for (ol_receiver_t * const *it = __ol_receivers_start;
         it != __ol_receivers_end; ++it) {
        if (*it && (*it)->name && strcmp((*it)->name, name) == 0) {
            return *it;
        }
    }
    return NULL;
}

static int copy_partition(const ol_partition_t *src, uint32_t src_off,
                          const ol_partition_t *dst, uint32_t dst_off,
                          uint32_t len)
{
    uint8_t  buf[OPENLOAD_COPY_CHUNK_SIZE];
    uint32_t done = 0;
    while (done < len) {
        uint32_t n = len - done;
        if (n > sizeof(buf)) { n = sizeof(buf); }
        int rc = ol_part_read(src, src_off + done, buf, n);
        if (rc != OL_OK) { return rc; }
        rc = ol_part_write(dst, dst_off + done, buf, n);
        if (rc != OL_OK) { return rc; }
        done += n;
    }
    return OL_OK;
}

int ol_updater_install(const char *staging_part, const char *target_part)
{
    const ol_partition_t *src = ol_part_find(staging_part);
    const ol_partition_t *dst = ol_part_find(target_part);
    if (!src || !dst) { return OL_E_PART_NOT_FOUND; }

    OL_LOGI("verify staging %s", src->name);
    int rc = ol_image_verify(src);
    if (rc != OL_OK) { return rc; }

    ol_image_header_t hdr;
    rc = ol_image_read_header(src, &hdr);
    if (rc != OL_OK) { return rc; }

    /* 防回滚 (可选). 当前 target 也必须含有效 header. */
#if OPENLOAD_ANTI_ROLLBACK
    {
        ol_image_header_t cur;
        if (ol_image_read_header(dst, &cur) == OL_OK &&
            hdr.firmware_version < cur.firmware_version) {
            OL_LOGE("anti-rollback: new=%u cur=%u",
                    hdr.firmware_version, cur.firmware_version);
            return OL_E_IMAGE_VERSION;
        }
    }
#endif

    uint32_t payload  = hdr.firmware_size;
    uint32_t total    = OL_IMAGE_HDR_SIZE + payload;
    if (total > dst->size) { return OL_E_IMAGE_SIZE; }

    /* erase 必须按 target device 的 sector_size 对齐, 否则底层驱动会拒绝.
       向上取整到 sector boundary, 但夹在 dst->size 内. */
    ol_flash_dev_t *dst_dev = ol_part_get_device(dst);
    uint32_t sector    = (dst_dev && dst_dev->sector_size) ? dst_dev->sector_size : 1;
    uint32_t erase_len = (total + sector - 1) & ~(sector - 1);
    if (erase_len > dst->size) { erase_len = dst->size; }

    OL_LOGI("erase target %s (%lu bytes, sector=%lu)",
            dst->name, erase_len, sector);
    rc = ol_part_erase(dst, 0, erase_len);
    if (rc != OL_OK) { return rc; }

    OL_LOGI("copy %lu bytes -> %s", total, dst->name);
    rc = copy_partition(src, 0, dst, 0, total);
    if (rc != OL_OK) { return rc; }

    OL_LOGI("verify target");
    rc = ol_image_verify(dst);
    if (rc != OL_OK) { return rc; }

    OL_LOGI("install ok");
    return OL_OK;
}

int ol_updater_backup(const char *target_part, const char *backup_part)
{
    const ol_partition_t *src = ol_part_find(target_part);
    const ol_partition_t *dst = ol_part_find(backup_part);
    if (!src || !dst) { return OL_E_PART_NOT_FOUND; }
    ol_image_header_t hdr;
    int rc = ol_image_read_header(src, &hdr);
    if (rc != OL_OK) { return rc; }
    uint32_t total = OL_IMAGE_HDR_SIZE + hdr.firmware_size;
    if (total > dst->size) { return OL_E_IMAGE_SIZE; }
    rc = ol_part_erase(dst, 0, total);
    if (rc != OL_OK) { return rc; }
    return copy_partition(src, 0, dst, 0, total);
}

int ol_updater_rollback(const char *backup_part, const char *target_part)
{
    return ol_updater_install(backup_part, target_part);
}

int ol_updater_run(const char *receiver_name,
                   const char *staging_part,
                   const char *target_part,
                   const char *url_or_null)
{
    ol_receiver_t *r = ol_receiver_find(receiver_name);
    if (!r) {
        OL_LOGE("receiver %s not found", receiver_name);
        return OL_E_NOT_FOUND;
    }
    const ol_partition_t *staging = ol_part_find(staging_part ? staging_part : target_part);
    if (!staging) { return OL_E_PART_NOT_FOUND; }
    ol_io_dev_t *io = ol_io_dev_find("console");

    /* HTTP / 其它需要外部参数的 receiver 在 begin 前注入 (e.g. URL). */
    if (url_or_null && r->ops->prepare) {
        int rc = r->ops->prepare(r, url_or_null);
        if (rc != OL_OK) {
            OL_LOGE("prepare: %s", ol_strerror(rc));
            return rc;
        }
    }

    OL_LOGI("erase staging %s", staging->name);
    int rc = ol_part_erase_all(staging);
    if (rc != OL_OK) { return rc; }

    rc = r->ops->begin(r, io, staging);
    if (rc != OL_OK) {
        r->ops->end(r);
        return rc;
    }
    while ((rc = r->ops->poll(r)) == 0) {
        /* spin; protocols 自带 timeout */
    }
    r->ops->end(r);
    if (rc < 0) {
        OL_LOGE("receive failed: %s", ol_strerror(rc));
        return rc;
    }

    /* receive 完成 (rc == 1), 进入 install. 若 staging==target 则跳过. */
    if (staging_part && target_part && strcmp(staging_part, target_part) != 0) {
        return ol_updater_install(staging_part, target_part);
    }
    return ol_image_verify(staging);
}
