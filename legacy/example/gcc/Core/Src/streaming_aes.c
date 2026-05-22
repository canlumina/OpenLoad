#include "streaming_aes.h"
#include "firmware_aes.h"
#include <string.h>

/* 简化的流式AES实现 - 直接使用firmware_aes函数 */

bool streaming_aes_init(streaming_aes_ctx_t* ctx, const uint8_t* key, const uint8_t* iv)
{
    if (!ctx || !key || !iv) {
        return false;
    }
    
    memcpy(ctx->key, key, 16);
    memcpy(ctx->iv, iv, 16);
    ctx->initialized = firmware_aes_init(key);
    return ctx->initialized;
}

uint32_t streaming_aes_decrypt(streaming_aes_ctx_t* ctx, const uint8_t* input, uint8_t* output, uint32_t size)
{
    if (!ctx || !ctx->initialized || !input || !output || size == 0 || (size % 16) != 0) {
        return 0;
    }
    
    if (!firmware_aes_decrypt_cbc(input, output, size, ctx->iv)) {
        return 0;
    }
    
    /* 更新IV */
    if (size >= 16) {
        memcpy(ctx->iv, &input[size - 16], 16);
    }
    
    return size;
}

uint32_t streaming_aes_finalize(streaming_aes_ctx_t* ctx, uint8_t* output, uint32_t max_size)
{
    if (ctx) {
        ctx->initialized = false;
    }
    return max_size;
}