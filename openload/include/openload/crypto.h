/*
 * OpenLoad - Crypto 接口
 *
 * 各算法独立实现, 通过 OPENLOAD_ENABLE_* 编译期开关裁剪。
 * 未启用的算法整个 .c 文件不参与编译, 不占 ROM。
 */
#pragma once

#include <stdint.h>
#include "openload/config.h"
#include "openload/partition.h"

/* ---------- CRC32 (始终启用) ----------
 * 多项式 0xEDB88320 (反射), 标准 Ethernet/zlib CRC32。
 * 流式接口: 用 0 作为初值开始, 每次把上次返回值传回作为 init。
 */
uint32_t ol_crc32(uint32_t init, const void *data, uint32_t len);

/** 计算分区一段范围的 CRC32, 内部分块读取避免大缓冲. */
uint32_t ol_crc32_partition(const ol_partition_t *p,
                            uint32_t off, uint32_t len);

#if OPENLOAD_ENABLE_AES_128_CTR
/* ---------- AES-128-CTR ---------- */
typedef struct {
    uint8_t  round_keys[176];
    uint8_t  counter[16];
    uint8_t  stream[16];
    uint8_t  stream_pos;     /* 0..15, 16 = 需要生成下一块 */
} ol_aes_ctr_ctx_t;

/** key 长度固定 16 字节, iv 16 字节 (与固件头 aes_iv 一致). */
int  ol_aes_ctr_init(ol_aes_ctr_ctx_t *ctx,
                     const uint8_t key[16], const uint8_t iv[16]);
void ol_aes_ctr_xcrypt(ol_aes_ctr_ctx_t *ctx,
                       const uint8_t *in, uint8_t *out, uint32_t len);
#endif

#if OPENLOAD_ENABLE_SHA256
/* ---------- SHA-256 ---------- */
typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  buffer[64];
    uint32_t buflen;
} ol_sha256_ctx_t;

void ol_sha256_init(ol_sha256_ctx_t *ctx);
void ol_sha256_update(ol_sha256_ctx_t *ctx, const uint8_t *data, uint32_t len);
void ol_sha256_final(ol_sha256_ctx_t *ctx, uint8_t out[32]);
#endif
