/*
 * OpenLoad - 升级编排
 *
 * 策略 (config 控): STAGING (M1), STAGING + BACKUP (M3-2).
 *
 * STAGING: receiver → staging → verify → erase target → copy → verify target.
 * +BACKUP: install 前先 target → backup (软失败); install 失败自动从 backup
 *          rollback; 全程标 OL_MAGIC_INSTALLING, 中断重启 boot 阶段检测到
 *          INSTALLING 自动 rollback.
 */
#include "openload/updater.h"
#include "openload/receiver.h"
#include "openload/partition.h"
#include "openload/image.h"
#include "openload/logger.h"
#include "openload/errno.h"
#include "openload/config.h"
#include "openload/crypto.h"
#include "openload/ops/io_ops.h"
#include "openload/ops/flash_ops.h"
#include "openload/ops/sys_ops.h"
#if OPENLOAD_ENABLE_AES_128_CTR
#  include "aes.h"   /* third_party/tiny-aes, CMake 提供 include path */
#endif
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

#if OPENLOAD_ENABLE_AES_128_CTR
/* 用户工程通过 OPENLOAD_AES_KEY_BYTES 在 openload_config.h 提供 16 字节
 * AES-128 key (initializer list). 默认全 0 让构建不挂, 但运行时第一次
 * 解密就会因 CRC 不匹配失败 (默认 key 永远不会匹配真实加密 image). */
#ifndef OPENLOAD_AES_KEY_BYTES
#define OPENLOAD_AES_KEY_BYTES \
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, \
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
#endif
static const uint8_t g_aes_key[16] = { OPENLOAD_AES_KEY_BYTES };

/* 流式 AES-CTR 解密 staging payload → target payload, 同时累计明文 CRC32.
 * IV 取自 hdr.aes_iv, AES_ctx 内部维护 counter, 同一 ctx 连续多次
 * AES_CTR_xcrypt_buffer 会沿 counter 接着算, 不需要 caller 维护. */
static int decrypt_payload_to_target(const ol_partition_t *src,
                                     const ol_partition_t *dst,
                                     const ol_image_header_t *hdr,
                                     uint32_t *out_crc)
{
    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, g_aes_key, hdr->aes_iv);

    uint8_t  buf[OPENLOAD_COPY_CHUNK_SIZE];
    uint32_t off  = 0;
    uint32_t crc  = 0;
    uint32_t left = hdr->firmware_size;
    while (left) {
        uint32_t n = (left > sizeof(buf)) ? sizeof(buf) : left;
        int rc = ol_part_read(src, OL_IMAGE_HDR_SIZE + off, buf, n);
        if (rc != OL_OK) { return rc; }
        AES_CTR_xcrypt_buffer(&ctx, buf, n);
        rc = ol_part_write(dst, OL_IMAGE_HDR_SIZE + off, buf, n);
        if (rc != OL_OK) { return rc; }
        crc = ol_crc32(crc, buf, n);
        off  += n;
        left -= n;
    }
    *out_crc = crc;
    return OL_OK;
}
#endif

int ol_updater_install_ex(const char *staging_part, const char *target_part,
                          uint32_t flags)
{
    const ol_partition_t *src = ol_part_find(staging_part);
    const ol_partition_t *dst = ol_part_find(target_part);
    if (!src || !dst) { return OL_E_PART_NOT_FOUND; }

    /* 先读 hdr 确认完整性 (hdr_crc 校验), 才能区分明文 / 密文路径. */
    ol_image_header_t hdr;
    int rc = ol_image_read_header(src, &hdr);
    if (rc != OL_OK) { return rc; }

    int is_encrypted = (hdr.flags & OL_IMG_F_ENCRYPTED) != 0;
#if !OPENLOAD_ENABLE_AES_128_CTR
    if (is_encrypted) {
        OL_LOGE("staging is encrypted but AES support not built in");
        return OL_E_NOT_SUPPORTED;
    }
#endif

    /* 明文路径走完整 ol_image_verify (含 board_id + payload CRC).
     * 密文路径 payload 是密文, firmware_crc32 是明文 CRC, 完整 verify 必
     * 失败 — 改在解密时流式累计校验. 但 hdr CRC + board_id 现在就要查. */
    if (!is_encrypted) {
        OL_LOGI("verify staging %s", src->name);
        rc = ol_image_verify(src);
        if (rc != OL_OK) { return rc; }
    } else {
        OL_LOGI("staging %s encrypted, decrypt-on-fly", src->name);
#if OPENLOAD_BOARD_ID
        if (hdr.board_id != 0 && hdr.board_id != OPENLOAD_BOARD_ID) {
            return OL_E_IMAGE_BOARD;
        }
#endif
        if (hdr.firmware_size == 0 ||
            hdr.firmware_size > src->size - OL_IMAGE_HDR_SIZE) {
            return OL_E_IMAGE_SIZE;
        }
    }

    /* 防回滚. flags & FORCE 时单次允许覆盖 (CLI install ... force, 或
     * rollback 路径自动用 FORCE 把旧固件写回去). */
#if OPENLOAD_ANTI_ROLLBACK
    if (!(flags & OL_INSTALL_F_FORCE)) {
        ol_image_header_t cur;
        if (ol_image_read_header(dst, &cur) == OL_OK &&
            hdr.firmware_version < cur.firmware_version) {
            OL_LOGE("anti-rollback: new=%lu.%lu.%lu.%lu cur=%lu.%lu.%lu.%lu",
                    OL_IMG_VER_MAJOR(hdr.firmware_version),
                    OL_IMG_VER_MINOR(hdr.firmware_version),
                    OL_IMG_VER_PATCH(hdr.firmware_version),
                    OL_IMG_VER_BUILD(hdr.firmware_version),
                    OL_IMG_VER_MAJOR(cur.firmware_version),
                    OL_IMG_VER_MINOR(cur.firmware_version),
                    OL_IMG_VER_PATCH(cur.firmware_version),
                    OL_IMG_VER_BUILD(cur.firmware_version));
            return OL_E_IMAGE_VERSION;
        }
    } else {
        OL_LOGW("install: anti-rollback bypassed by force flag");
    }
#endif

    uint32_t payload  = hdr.firmware_size;
    uint32_t total    = OL_IMAGE_HDR_SIZE + payload;
    if (total > dst->size) { return OL_E_IMAGE_SIZE; }

    /* M3-2 backup-before. 在 erase target 之前抢救现 target 的有效固件到
     * backup 分区, 给后面回滚兜底. rollback 路径 (NO_BACKUP) 跳过, 否则会
     * 把 backup 自己覆盖. 软失败仅 WRN, 不阻断 install. */
    int did_backup = 0;
#if OPENLOAD_ENABLE_BACKUP
    if (!(flags & OL_INSTALL_F_NO_BACKUP)) {
        const ol_partition_t *bkp = ol_part_find("backup");
        if (!bkp) {
            OL_LOGW("backup partition not found, skip");
        } else if (ol_image_verify(dst) != OL_OK) {
            OL_LOGW("target has no valid image, skip backup");
        } else {
            OL_LOGI("backup %s -> backup", dst->name);
            int brc = ol_updater_backup(target_part, "backup");
            if (brc != OL_OK) {
                OL_LOGW("backup failed: %s (continue install)",
                        ol_strerror(brc));
            } else {
                did_backup = 1;
            }
        }
    }
#endif

    /* M3-2 INSTALLING magic: install 中断 → 下次启动 boot 检测到自动
     * rollback. 写入失败不阻断 (magic 不是必填 op). */
#if OPENLOAD_ENABLE_BACKUP
    (void)ol_magic_write(OL_MAGIC_INSTALLING);
#endif

    /* erase 必须按 target device 的 sector_size 对齐, 否则底层驱动会拒绝.
       向上取整到 sector boundary, 但夹在 dst->size 内. */
    ol_flash_dev_t *dst_dev = ol_part_get_device(dst);
    uint32_t sector    = (dst_dev && dst_dev->sector_size) ? dst_dev->sector_size : 1;
    uint32_t erase_len = (total + sector - 1) & ~(sector - 1);
    if (erase_len > dst->size) { erase_len = dst->size; }

    OL_LOGI("erase target %s (%lu bytes, sector=%lu)",
            dst->name, erase_len, sector);
    rc = ol_part_erase(dst, 0, erase_len);
    if (rc != OL_OK) { goto install_failed; }

    if (!is_encrypted) {
        OL_LOGI("copy %lu bytes -> %s", total, dst->name);
        rc = copy_partition(src, 0, dst, 0, total);
        if (rc != OL_OK) { goto install_failed; }
    } else {
#if OPENLOAD_ENABLE_AES_128_CTR
        /* 1. 解密 payload 流式落 target, 累计明文 CRC32. */
        OL_LOGI("decrypt %lu bytes -> %s", payload, dst->name);
        uint32_t plain_crc = 0;
        rc = decrypt_payload_to_target(src, dst, &hdr, &plain_crc);
        if (rc != OL_OK) { goto install_failed; }
        if (plain_crc != hdr.firmware_crc32) {
            OL_LOGE("decrypted CRC mismatch: calc=0x%08lx hdr=0x%08lx",
                    plain_crc, hdr.firmware_crc32);
            rc = OL_E_IMAGE_PAYLOAD_CRC;
            goto install_failed;
        }

        /* 2. 写 target hdr: 清 ENCRYPTED, 清 aes_iv, 重算 hdr_crc.
         * 这样 target 上是纯明文 image, boot 走标准 ol_image_verify 链. */
        ol_image_header_t out_hdr = hdr;
        out_hdr.flags &= ~OL_IMG_F_ENCRYPTED;
        memset(out_hdr.aes_iv, 0, sizeof(out_hdr.aes_iv));
        ol_image_seal_header(&out_hdr);
        rc = ol_part_write(dst, 0, (const uint8_t *)&out_hdr,
                           OL_IMAGE_HDR_SIZE);
        if (rc != OL_OK) { goto install_failed; }
#endif
    }

    OL_LOGI("verify target");
    rc = ol_image_verify(dst);
    if (rc != OL_OK) { goto install_failed; }

#if OPENLOAD_ENABLE_BACKUP
    (void)ol_magic_write(OL_MAGIC_NONE);
#endif
    OL_LOGI("install ok");
    return OL_OK;

install_failed:
#if OPENLOAD_ENABLE_BACKUP
    if (did_backup) {
        OL_LOGW("install failed (%s), auto rollback from backup",
                ol_strerror(rc));
        int rr = ol_updater_install_ex("backup", target_part,
                                       OL_INSTALL_F_FORCE |
                                       OL_INSTALL_F_NO_BACKUP);
        if (rr == OL_OK) {
            OL_LOGW("rollback ok, target restored to previous image");
        } else {
            OL_LOGE("rollback failed: %s", ol_strerror(rr));
        }
    }
    (void)ol_magic_write(OL_MAGIC_NONE);
#else
    (void)did_backup;
#endif
    return rc;
}

int ol_updater_install(const char *staging_part, const char *target_part)
{
    return ol_updater_install_ex(staging_part, target_part, 0);
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

    /* erase 按 image 大小 + sector 对齐, 不整盘擦. 448KB backup 整擦
     * 在 W25Q64 上 ~3-30s, 没必要. 老实现 ol_part_erase(dst, 0, total)
     * 不对齐会被 W25Q64 4KB sector 驱动拒绝, M1 erase bug 的同根. */
    ol_flash_dev_t *dst_dev = ol_part_get_device(dst);
    uint32_t sector    = (dst_dev && dst_dev->sector_size) ? dst_dev->sector_size : 1;
    uint32_t erase_len = (total + sector - 1) & ~(sector - 1);
    if (erase_len > dst->size) { erase_len = dst->size; }

    OL_LOGI("erase backup %s (%lu bytes, sector=%lu)",
            dst->name, erase_len, sector);
    rc = ol_part_erase(dst, 0, erase_len);
    if (rc != OL_OK) { return rc; }

    OL_LOGI("copy %lu bytes %s -> %s", total, src->name, dst->name);
    return copy_partition(src, 0, dst, 0, total);
}

int ol_updater_rollback(const char *backup_part, const char *target_part)
{
    /* FORCE: rollback 必然把旧版本写回, 跟防回滚天然冲突, 单次旁路.
     * NO_BACKUP: 不要把当前 (可能损坏的) target 再 backup 一次, 否则
     * 把 backup 里仅存的"上一个好镜像"给覆盖了. */
    return ol_updater_install_ex(backup_part, target_part,
                                 OL_INSTALL_F_FORCE | OL_INSTALL_F_NO_BACKUP);
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
