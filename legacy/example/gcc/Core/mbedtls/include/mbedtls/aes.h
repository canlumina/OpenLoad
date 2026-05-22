#ifndef MBEDTLS_AES_H
#define MBEDTLS_AES_H

#include <stddef.h>
#include <stdint.h>

/* 错误码 */
#define MBEDTLS_ERR_AES_INVALID_KEY_LENGTH                -0x0020
#define MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH              -0x0022

/* AES密钥长度 */
#define MBEDTLS_AES_ENCRYPT     1
#define MBEDTLS_AES_DECRYPT     0

#define MBEDTLS_AES_KEYSIZE_128 16
#define MBEDTLS_AES_KEYSIZE_192 24
#define MBEDTLS_AES_KEYSIZE_256 32

#define MBEDTLS_AES_BLOCKSIZE   16

/* AES上下文结构 */
typedef struct mbedtls_aes_context
{
    int nr;                     /* 轮数 */
    uint32_t *rk;              /* AES轮密钥 */
    uint32_t buf[68];          /* 存储轮密钥的缓冲区 */
} __attribute__((aligned(4))) mbedtls_aes_context;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化AES上下文
 * @param ctx AES上下文
 */
void mbedtls_aes_init(mbedtls_aes_context *ctx);

/**
 * @brief 释放AES上下文
 * @param ctx AES上下文
 */
void mbedtls_aes_free(mbedtls_aes_context *ctx);

/**
 * @brief 设置加密密钥
 * @param ctx AES上下文
 * @param key 密钥
 * @param keybits 密钥长度（位）
 * @return 0表示成功
 */
int mbedtls_aes_setkey_enc(mbedtls_aes_context *ctx,
                          const unsigned char *key,
                          unsigned int keybits);

/**
 * @brief 设置解密密钥
 * @param ctx AES上下文
 * @param key 密钥
 * @param keybits 密钥长度（位）
 * @return 0表示成功
 */
int mbedtls_aes_setkey_dec(mbedtls_aes_context *ctx,
                          const unsigned char *key,
                          unsigned int keybits);

/**
 * @brief AES ECB加密/解密单个块
 * @param ctx AES上下文
 * @param mode MBEDTLS_AES_ENCRYPT或MBEDTLS_AES_DECRYPT
 * @param input 输入数据（16字节）
 * @param output 输出数据（16字节）
 * @return 0表示成功
 */
int mbedtls_aes_crypt_ecb(mbedtls_aes_context *ctx,
                         int mode,
                         const unsigned char input[16],
                         unsigned char output[16]);

/**
 * @brief AES CBC加密/解密
 * @param ctx AES上下文
 * @param mode MBEDTLS_AES_ENCRYPT或MBEDTLS_AES_DECRYPT
 * @param length 输入数据长度（必须是16的倍数）
 * @param iv 初始化向量（16字节，会被修改）
 * @param input 输入数据
 * @param output 输出数据
 * @return 0表示成功
 */
int mbedtls_aes_crypt_cbc(mbedtls_aes_context *ctx,
                         int mode,
                         size_t length,
                         unsigned char iv[16],
                         const unsigned char *input,
                         unsigned char *output);

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_AES_H */