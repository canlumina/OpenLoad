/*
 * OpenLoad - Image 头校验
 *
 * 校验链 (按配置叠加):
 *   1. magic / hdr_crc32  (M1, 防头部损坏)
 *   2. board_id          (M1, 跨型号保护)
 *   3. payload CRC32      (M1, 防传输错误)
 *   4. payload SHA-256[16] (M4-1, 防碰撞 + 为 Ed25519 摘要; 仅在
 *      hdr.firmware_sha256 非全 0 时检查, 容许逐项启用)
 */
#include "openload/image.h"
#include "openload/partition.h"
#include "openload/crypto.h"
#include "openload/errno.h"
#include "openload/config.h"
#if OPENLOAD_ENABLE_SHA256
#  include "sha256.h"   /* third_party/sha256, CMake 提供 include path */
#endif
#include <string.h>

int ol_image_read_header(const ol_partition_t *p, ol_image_header_t *hdr_out)
{
    if (!p || !hdr_out) { return OL_E_INVAL; }
    if (p->size < OL_IMAGE_HDR_SIZE) { return OL_E_IMAGE_SIZE; }

    int rc = ol_part_read(p, 0, hdr_out, sizeof(*hdr_out));
    if (rc != OL_OK) { return rc; }

    if (hdr_out->magic != OL_IMAGE_MAGIC) {
        return OL_E_IMAGE_MAGIC;
    }

    /* 头部前 60 字节 CRC, 最后 4 字节是 hdr_crc32 本身 */
    uint32_t calc = ol_crc32(0, hdr_out, OL_IMAGE_HDR_SIZE - sizeof(uint32_t));
    if (calc != hdr_out->hdr_crc32) {
        return OL_E_IMAGE_HDR_CRC;
    }
    return OL_OK;
}

#if OPENLOAD_ENABLE_SHA256
/* 流式从 partition 按 chunk 算 SHA-256, 截取前 16 字节存 out[16]. */
static int sha256_partition_first16(const ol_partition_t *p,
                                    uint32_t off, uint32_t len,
                                    uint8_t out[16])
{
    SHA256_CTX ctx;
    sha256_init(&ctx);

    uint8_t  buf[128];
    uint32_t done = 0;
    while (done < len) {
        uint32_t n = len - done;
        if (n > sizeof(buf)) { n = sizeof(buf); }
        int rc = ol_part_read(p, off + done, buf, n);
        if (rc != OL_OK) { return rc; }
        sha256_update(&ctx, buf, n);
        done += n;
    }
    uint8_t full[32];
    sha256_final(&ctx, full);
    memcpy(out, full, 16);
    return OL_OK;
}
#endif

int ol_image_verify(const ol_partition_t *p)
{
    ol_image_header_t hdr;
    int rc = ol_image_read_header(p, &hdr);
    if (rc != OL_OK) { return rc; }

    /* board_id 检查: 0 = 不检查 (跨板通用固件) */
#if OPENLOAD_BOARD_ID
    if (hdr.board_id != 0 && hdr.board_id != OPENLOAD_BOARD_ID) {
        return OL_E_IMAGE_BOARD;
    }
#endif

    if (hdr.firmware_size == 0 ||
        hdr.firmware_size > p->size - OL_IMAGE_HDR_SIZE) {
        return OL_E_IMAGE_SIZE;
    }

    uint32_t calc = ol_crc32_partition(p, OL_IMAGE_HDR_SIZE, hdr.firmware_size);
    if (calc != hdr.firmware_crc32) {
        return OL_E_IMAGE_PAYLOAD_CRC;
    }

#if OPENLOAD_ENABLE_SHA256
    /* 仅在 hdr.firmware_sha256 非全 0 时叠加 SHA-256 校验.
     * 让老固件 (无 SHA 字段) 仍能通过, 新固件可逐步上 SHA. */
    static const uint8_t zero16[16] = { 0 };
    if (memcmp(hdr.firmware_sha256, zero16, 16) != 0) {
        uint8_t calc_sha[16];
        rc = sha256_partition_first16(p, OL_IMAGE_HDR_SIZE,
                                      hdr.firmware_size, calc_sha);
        if (rc != OL_OK) { return rc; }
        if (memcmp(calc_sha, hdr.firmware_sha256, 16) != 0) {
            return OL_E_IMAGE_HASH;
        }
    }
#endif

    return OL_OK;
}

void ol_image_seal_header(ol_image_header_t *hdr)
{
    if (!hdr) { return; }
    hdr->hdr_crc32 = 0;
    hdr->hdr_crc32 = ol_crc32(0, hdr, OL_IMAGE_HDR_SIZE - sizeof(uint32_t));
}
