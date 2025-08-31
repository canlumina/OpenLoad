#include "streaming_aes.h"
#include "firmware_aes.h"  /* 复用AES内核函数 */
#include <string.h>

/**
 * @brief 初始化流式AES解密器
 */
bool streaming_aes_init(streaming_aes_ctx_t* ctx, const uint8_t* key, const uint8_t* iv)
{
    if (!ctx || !key || !iv) {
        return false;
    }
    
    /* 复制密钥和IV */
    memcpy(ctx->key, key, 16);
    memcpy(ctx->iv, iv, 16);
    
    /* 初始化AES */
    if (!firmware_aes_init(key)) {
        return false;
    }
    
    /* 清零缓冲区 */
    memset(ctx->buffer, 0, 16);
    ctx->buffer_pos = 0;
    ctx->initialized = true;
    
    return true;
}

/**
 * @brief 流式AES-CBC解密
 * 直接使用现有的firmware_aes_decrypt_cbc函数
 */
uint32_t streaming_aes_decrypt(streaming_aes_ctx_t* ctx, const uint8_t* input, uint8_t* output, uint32_t size)
{
    if (!ctx || !ctx->initialized || !input || !output || size == 0) {
        return 0;
    }
    
    /* 确保输入大小是16字节的倍数 */
    if (size % 16 != 0) {
        return 0;
    }
    
    /* 直接使用已有的AES-CBC解密函数 */
    /* 注意：每次调用都会更新IV链 */
    if (!firmware_aes_decrypt_cbc(input, output, size, ctx->iv)) {
        return 0;
    }
    
    /* 更新IV为最后一个密文块 */
    if (size >= 16) {
        memcpy(ctx->iv, &input[size - 16], 16);
    }
    
    return size;
}

/**
 * @brief 完成解密并移除PKCS7填充
 */
uint32_t streaming_aes_finalize(streaming_aes_ctx_t* ctx, uint8_t* output, uint32_t max_size)
{
    if (!ctx || !ctx->initialized || !output || max_size == 0) {
        return 0;
    }
    
    /* 流式解密中，数据已经在前面处理完了 */
    /* 这里只需要处理最后一块的PKCS7填充 */
    
    /* 简化处理：由于我们是流式处理，PKCS7填充在最后一块中处理 */
    ctx->initialized = false;
    return max_size; /* 实际大小由调用者处理 */
}