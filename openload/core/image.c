/*
 * OpenLoad - Image 头校验
 *
 * 校验链 (按配置叠加):
 *   1. magic / hdr_crc32  (M1, 防头部损坏)
 *   2. board_id          (M1, 跨型号保护)
 *   3. payload CRC32      (M1, 防传输错误)
 *   4. payload SHA-256[16] (M4-1, 防碰撞 + 为 Ed25519 摘要; 仅在
 *      hdr.firmware_sha256 非全 0 时检查, 容许逐项启用)
 *   5. Ed25519(SHA-256) 签名 (M4-2, 仅在 hdr.flags & OL_IMG_F_SIGNED
 *      且 OPENLOAD_ENABLE_ED25519 时检查; 签名 64 字节追在 payload 末尾)
 */
#include "openload/image.h"
#include "openload/partition.h"
#include "openload/crypto.h"
#include "openload/errno.h"
#include "openload/config.h"
#include "openload/logger.h"
#if OPENLOAD_ENABLE_SHA256
#  include "sha256.h"   /* third_party/sha256, CMake 提供 include path */
#endif
#if OPENLOAD_ENABLE_ED25519
#  include "tweetnacl.h" /* third_party/tweetnacl */
#endif
#include <string.h>

#if OPENLOAD_ENABLE_ED25519
/* 用户工程通过 OPENLOAD_ED25519_PUBKEY_BYTES 提供 32 字节 Ed25519 公钥
 * (initializer list). 默认全 0 让构建不挂; 全 0 公钥 verify 永远不会通过. */
#ifndef OPENLOAD_ED25519_PUBKEY_BYTES
#define OPENLOAD_ED25519_PUBKEY_BYTES \
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, \
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, \
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, \
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
#endif
static const uint8_t g_ed25519_pubkey[32] = { OPENLOAD_ED25519_PUBKEY_BYTES };
#endif

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
/* 流式从 partition 按 chunk 算 SHA-256, 输出完整 32 字节.
 * 调用方按需取前 16 字节比对 hdr.firmware_sha256, 完整 32 字节给 Ed25519 用. */
static int sha256_partition_full(const ol_partition_t *p,
                                 uint32_t off, uint32_t len,
                                 uint8_t out[32])
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
    sha256_final(&ctx, out);
    return OL_OK;
}
#endif

#if OPENLOAD_ENABLE_ED25519
/* tweetnacl crypto_sign_open 接口: sm = signature(64) || message,
 * 验证通过则把 message 写入 m, 返回 0. 我们消息 = SHA-256 摘要 (32 字节). */
static int verify_ed25519_sig(const ol_partition_t *p,
                              const ol_image_header_t *hdr,
                              const uint8_t sha32[32])
{
    uint8_t sm[64 + 32];
    int rc = ol_part_read(p, OL_IMAGE_HDR_SIZE + hdr->firmware_size,
                          sm, 64);
    if (rc != OL_OK) { return rc; }
    memcpy(sm + 64, sha32, 32);

    uint8_t out_msg[64 + 32];
    unsigned long long out_len = 0;
    if (crypto_sign_open(out_msg, &out_len, sm, sizeof(sm),
                         g_ed25519_pubkey) != 0) {
        return OL_E_IMAGE_SIGNATURE;
    }
    return OL_OK;
}
#endif

int ol_image_verify(const ol_partition_t *p)
{
    ol_image_header_t hdr;
    int rc = ol_image_read_header(p, &hdr);
    if (rc != OL_OK) { return rc; }

    /* board_id 检查: 主 board_id == 0 跨板通用; 否则任一非 0 槽匹配即通过
     * (M6-2 多 board_id 支持, 老 image extra=0 仍按主 board_id 严匹配). */
#if OPENLOAD_BOARD_ID
    if (hdr.board_id != 0) {
        const uint16_t slots[3] = {
            hdr.board_id, hdr.board_id_extra[0], hdr.board_id_extra[1]
        };
        int match = 0;
        for (int i = 0; i < 3; ++i) {
            if (slots[i] != 0 && slots[i] == OPENLOAD_BOARD_ID) {
                match = 1;
                break;
            }
        }
        if (!match) { return OL_E_IMAGE_BOARD; }
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
    /* 仅在 hdr.firmware_sha256 非全 0 或 SIGNED 时算 SHA.
     * 算完整 32 字节, hdr 比对前 16 字节, 完整字节供下面 Ed25519 用. */
    static const uint8_t zero16[16] = { 0 };
    int need_sha = (memcmp(hdr.firmware_sha256, zero16, 16) != 0);
#  if OPENLOAD_ENABLE_ED25519
    if (hdr.flags & OL_IMG_F_SIGNED) { need_sha = 1; }
#  endif
    uint8_t sha32[32];
    if (need_sha) {
        rc = sha256_partition_full(p, OL_IMAGE_HDR_SIZE,
                                   hdr.firmware_size, sha32);
        if (rc != OL_OK) { return rc; }
        if (memcmp(hdr.firmware_sha256, zero16, 16) != 0 &&
            memcmp(sha32, hdr.firmware_sha256, 16) != 0) {
            return OL_E_IMAGE_HASH;
        }
    }
#endif

#if OPENLOAD_ENABLE_ED25519
    if (hdr.flags & OL_IMG_F_SIGNED) {
#  if !OPENLOAD_ENABLE_SHA256
#    error "OPENLOAD_ENABLE_ED25519 requires OPENLOAD_ENABLE_SHA256"
#  endif
        /* 签名校验需要 image 末尾还能放下 64 字节 sig */
        if (OL_IMAGE_HDR_SIZE + hdr.firmware_size + 64 > p->size) {
            return OL_E_IMAGE_SIZE;
        }
        rc = verify_ed25519_sig(p, &hdr, sha32);
        if (rc != OL_OK) { return rc; }
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
