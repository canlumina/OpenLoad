/*
 * OpenLoad - 固件头格式
 *
 * 固件文件布局: [ ol_image_header_t (64B) ][ payload (firmware_size 字节) ]
 *
 * 头部由 PC 端 tools/image_tool.py 生成。bootloader 在 jump 前必须校验
 * magic / hdr_crc32 / payload crc32 (启用时再追加 sha256 / signature)。
 */
#pragma once

#include <stdint.h>
#include "openload/partition.h"

#define OL_IMAGE_MAGIC      0x4F4C4F41u   /* "AOLO" little-endian, 用于识别固件 */
#define OL_IMAGE_HDR_SIZE   64u
#define OL_IMAGE_FMT_VER    1u

/* image flags */
#define OL_IMG_F_ENCRYPTED  (1u << 0)
#define OL_IMG_F_SIGNED     (1u << 1)

typedef struct {
    uint32_t magic;             /* OL_IMAGE_MAGIC */
    uint8_t  hdr_version;       /* = OL_IMAGE_FMT_VER */
    uint8_t  flags;             /* bit0 encrypted, bit1 signed */
    uint16_t board_id;          /* 用户自定义, 跨型号刷固件保护 */

    uint32_t firmware_size;     /* 原始 (解密后) payload 字节数 */
    uint32_t firmware_crc32;    /* 原始 payload CRC32 */
    uint32_t firmware_version;  /* major:8 minor:8 patch:8 build:8 */
    uint32_t build_timestamp;   /* Unix epoch */

    uint8_t  firmware_sha256[16]; /* 截断的 SHA-256 前 16 字节, 未启用为 0 */
    uint8_t  aes_iv[16];          /* AES-CTR IV, 未启用为 0 */

    uint8_t  reserved[4];         /* 预留, 必须为 0 (供 v2 签名扩展) */

    uint32_t hdr_crc32;         /* 头部前 60 字节的 CRC32, 防头部损坏误判 */
} __attribute__((packed)) ol_image_header_t;

_Static_assert(sizeof(ol_image_header_t) == OL_IMAGE_HDR_SIZE,
               "ol_image_header_t size != 64");

/* 版本号便捷打包/解包 */
#define OL_IMG_VER_PACK(maj, min, patch, build) \
    (((uint32_t)(maj)   << 24) | \
     ((uint32_t)(min)   << 16) | \
     ((uint32_t)(patch) <<  8) | \
     ((uint32_t)(build) <<  0))
#define OL_IMG_VER_MAJOR(v)  (((v) >> 24) & 0xFFu)
#define OL_IMG_VER_MINOR(v)  (((v) >> 16) & 0xFFu)
#define OL_IMG_VER_PATCH(v)  (((v) >>  8) & 0xFFu)
#define OL_IMG_VER_BUILD(v)  (((v) >>  0) & 0xFFu)

/**
 * @brief 从分区起始 (offset 0) 读取并校验固件头.
 * @param  hdr_out  输出, 调用方提供 64 字节缓冲.
 * @return OL_OK / OL_E_IMAGE_MAGIC / OL_E_IMAGE_HDR_CRC / IO 错误.
 */
int ol_image_read_header(const ol_partition_t *p, ol_image_header_t *hdr_out);

/**
 * @brief 完整校验分区固件: 头部 + payload CRC + (启用时) board_id 匹配.
 * @return OL_OK 表示通过, 其它为各类校验失败码.
 */
int ol_image_verify(const ol_partition_t *p);

/**
 * @brief 计算并写入头部 CRC (仅在 host 端 image_tool 使用; 设备端通常不需要).
 */
void ol_image_seal_header(ol_image_header_t *hdr);
