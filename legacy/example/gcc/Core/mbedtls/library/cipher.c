#include "mbedtls/cipher.h"
#include "mbedtls/aes.h"
#include <string.h>

/* AES cipher 信息结构 */
static const mbedtls_cipher_info_t aes_128_cbc_info = {
    MBEDTLS_CIPHER_AES_128_CBC,
    MBEDTLS_MODE_CBC,
    128,
    "AES-128-CBC",
    0,
    0,
    16,
    &mbedtls_aes_info
};

static const mbedtls_cipher_info_t *cipher_definitions[] = {
    &aes_128_cbc_info,
    NULL
};

const mbedtls_cipher_info_t *mbedtls_cipher_info_from_type(const mbedtls_cipher_type_t cipher_type)
{
    const mbedtls_cipher_info_t *def;
    
    for (int i = 0; cipher_definitions[i] != NULL; i++) {
        def = cipher_definitions[i];
        if (def->type == cipher_type) {
            return def;
        }
    }
    
    return NULL;
}

void mbedtls_cipher_init(mbedtls_cipher_context_t *ctx)
{
    memset(ctx, 0, sizeof(mbedtls_cipher_context_t));
}

void mbedtls_cipher_free(mbedtls_cipher_context_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    
    if (ctx->cipher_ctx) {
        if (ctx->cipher_info && ctx->cipher_info->base_idx == &mbedtls_aes_info) {
            mbedtls_aes_free((mbedtls_aes_context *)ctx->cipher_ctx);
        }
        mbedtls_platform_zeroize(ctx->cipher_ctx, sizeof(mbedtls_aes_context));
        mbedtls_free(ctx->cipher_ctx);
    }
    
    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_cipher_context_t));
}

int mbedtls_cipher_setup(mbedtls_cipher_context_t *ctx, const mbedtls_cipher_info_t *cipher_info)
{
    if (cipher_info == NULL) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }
    
    memset(ctx, 0, sizeof(mbedtls_cipher_context_t));
    
    if (cipher_info->base_idx == &mbedtls_aes_info) {
        ctx->cipher_ctx = mbedtls_calloc(1, sizeof(mbedtls_aes_context));
        if (ctx->cipher_ctx == NULL) {
            return MBEDTLS_ERR_CIPHER_ALLOC_FAILED;
        }
        
        mbedtls_aes_init((mbedtls_aes_context *)ctx->cipher_ctx);
    }
    
    ctx->cipher_info = cipher_info;
    
    return 0;
}

int mbedtls_cipher_setkey(mbedtls_cipher_context_t *ctx, const unsigned char *key,
                         int key_bitlen, const mbedtls_operation_t operation)
{
    if (ctx->cipher_info == NULL) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }
    
    if ((unsigned int)key_bitlen != ctx->cipher_info->key_bitlen) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }
    
    ctx->key_bitlen = key_bitlen;
    ctx->operation = operation;
    
    if (ctx->cipher_info->base_idx == &mbedtls_aes_info) {
        if (operation == MBEDTLS_ENCRYPT) {
            return mbedtls_aes_setkey_enc((mbedtls_aes_context *)ctx->cipher_ctx, key, key_bitlen);
        } else {
            return mbedtls_aes_setkey_dec((mbedtls_aes_context *)ctx->cipher_ctx, key, key_bitlen);
        }
    }
    
    return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
}

int mbedtls_cipher_set_iv(mbedtls_cipher_context_t *ctx, const unsigned char *iv, size_t iv_len)
{
    size_t actual_iv_size = ctx->cipher_info->iv_size;
    
    if (iv_len != actual_iv_size) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }
    
    memcpy(ctx->iv, iv, actual_iv_size);
    ctx->iv_size = actual_iv_size;
    
    return 0;
}

int mbedtls_cipher_reset(mbedtls_cipher_context_t *ctx)
{
    if (ctx->cipher_info == NULL) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }
    
    ctx->unprocessed_len = 0;
    
    return 0;
}

int mbedtls_cipher_update(mbedtls_cipher_context_t *ctx, const unsigned char *input,
                         size_t ilen, unsigned char *output, size_t *olen)
{
    int ret = 0;
    size_t block_size;
    
    if (ctx->cipher_info == NULL) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }
    
    *olen = 0;
    block_size = ctx->cipher_info->block_size;
    
    if (ctx->cipher_info->mode == MBEDTLS_MODE_CBC) {
        /*
         * 如果有缓存数据，先处理
         */
        if (ctx->unprocessed_len != 0) {
            size_t use_len = block_size - ctx->unprocessed_len;
            
            if (use_len > ilen) {
                use_len = ilen;
            }
            
            memcpy(&(ctx->unprocessed_data[ctx->unprocessed_len]), input, use_len);
            
            input += use_len;
            ilen -= use_len;
            ctx->unprocessed_len += use_len;
            
            if (ctx->unprocessed_len == block_size) {
                if (ctx->cipher_info->base_idx == &mbedtls_aes_info) {
                    ret = mbedtls_aes_crypt_cbc((mbedtls_aes_context *)ctx->cipher_ctx,
                                               ctx->operation, block_size, ctx->iv,
                                               ctx->unprocessed_data, output);
                    if (ret != 0) {
                        return ret;
                    }
                }
                
                *olen += block_size;
                output += block_size;
                ctx->unprocessed_len = 0;
            }
        }
        
        /*
         * 处理完整的块
         */
        if (ilen >= block_size) {
            size_t nb_blocks = ilen / block_size;
            size_t extra_bytes = ilen % block_size;
            
            if (ctx->cipher_info->base_idx == &mbedtls_aes_info) {
                ret = mbedtls_aes_crypt_cbc((mbedtls_aes_context *)ctx->cipher_ctx,
                                           ctx->operation, nb_blocks * block_size,
                                           ctx->iv, input, output);
                if (ret != 0) {
                    return ret;
                }
            }
            
            *olen += nb_blocks * block_size;
            input += nb_blocks * block_size;
            output += nb_blocks * block_size;
            ilen = extra_bytes;
        }
        
        /*
         * 缓存剩余数据
         */
        if (ilen > 0) {
            memcpy(ctx->unprocessed_data, input, ilen);
            ctx->unprocessed_len = ilen;
        }
    }
    
    return 0;
}

int mbedtls_cipher_finish(mbedtls_cipher_context_t *ctx, unsigned char *output, size_t *olen)
{
    if (ctx->cipher_info == NULL) {
        return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }
    
    *olen = 0;
    
    if (ctx->unprocessed_len != 0) {
        return MBEDTLS_ERR_CIPHER_FULL_BLOCK_EXPECTED;
    }
    
    return 0;
}

int mbedtls_cipher_crypt(mbedtls_cipher_context_t *ctx,
                        const unsigned char *iv, size_t iv_len,
                        const unsigned char *input, size_t ilen,
                        unsigned char *output, size_t *olen)
{
    int ret;
    size_t finish_olen;
    
    if ((ret = mbedtls_cipher_set_iv(ctx, iv, iv_len)) != 0) {
        return ret;
    }
    
    if ((ret = mbedtls_cipher_reset(ctx)) != 0) {
        return ret;
    }
    
    if ((ret = mbedtls_cipher_update(ctx, input, ilen, output, olen)) != 0) {
        return ret;
    }
    
    if ((ret = mbedtls_cipher_finish(ctx, output + *olen, &finish_olen)) != 0) {
        return ret;
    }
    
    *olen += finish_olen;
    
    return 0;
}

const mbedtls_cipher_base_t mbedtls_aes_info = {
    MBEDTLS_CIPHER_ID_AES,
    NULL, /* encrypt_func */
    NULL, /* decrypt_func */
    NULL, /* setkey_enc_func */
    NULL, /* setkey_dec_func */
    NULL, /* ctx_alloc_func */
    NULL  /* ctx_free_func */
};