#ifndef STREAMING_AES_H
#define STREAMING_AES_H

#include <stdint.h>
#include <stdbool.h>

/* 流式AES解密器 */
typedef struct {
    uint8_t key[16];                    /* AES密钥 */
    uint8_t iv[16];                     /* 当前IV */
    uint32_t round_keys[44];            /* 扩展密钥 */
    uint8_t buffer[16];                 /* 块缓冲区 */
    uint32_t buffer_pos;                /* 缓冲区位置 */
    bool initialized;                   /* 是否已初始化 */
} streaming_aes_ctx_t;

/**
 * @brief 初始化流式AES解密器
 * @param ctx 解密器上下文
 * @param key AES密钥（16字节）
 * @param iv 初始化向量（16字节）
 * @return true=成功，false=失败
 */
bool streaming_aes_init(streaming_aes_ctx_t* ctx, const uint8_t* key, const uint8_t* iv);

/**
 * @brief 流式解密数据
 * @param ctx 解密器上下文
 * @param input 输入数据
 * @param output 输出缓冲区
 * @param size 数据大小（必须是16的倍数）
 * @return 解密后的数据大小
 */
uint32_t streaming_aes_decrypt(streaming_aes_ctx_t* ctx, const uint8_t* input, uint8_t* output, uint32_t size);

/**
 * @brief 完成解密并处理填充
 * @param ctx 解密器上下文
 * @param output 输出缓冲区
 * @param max_size 输出缓冲区最大大小
 * @return 最终解密数据大小，0=失败
 */
uint32_t streaming_aes_finalize(streaming_aes_ctx_t* ctx, uint8_t* output, uint32_t max_size);

#endif /* STREAMING_AES_H */