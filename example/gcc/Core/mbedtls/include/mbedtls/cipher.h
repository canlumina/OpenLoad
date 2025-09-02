#ifndef MBEDTLS_CIPHER_H
#define MBEDTLS_CIPHER_H

#include "mbedtls/mbedtls_config.h"
#include "mbedtls/platform.h"
#include <stddef.h>
#include <stdint.h>

#define MBEDTLS_CIPHER_ID_AES            2

#define MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA         -0x6280
#define MBEDTLS_ERR_CIPHER_ALLOC_FAILED          -0x6300
#define MBEDTLS_ERR_CIPHER_FULL_BLOCK_EXPECTED   -0x6380

typedef enum {
    MBEDTLS_CIPHER_NONE = 0,
    MBEDTLS_CIPHER_AES_128_CBC,
} mbedtls_cipher_type_t;

typedef enum {
    MBEDTLS_MODE_NONE = 0,
    MBEDTLS_MODE_CBC,
    MBEDTLS_MODE_CTR,
} mbedtls_cipher_mode_t;

typedef enum {
    MBEDTLS_OPERATION_NONE = -1,
    MBEDTLS_DECRYPT = 0,
    MBEDTLS_ENCRYPT,
} mbedtls_operation_t;

typedef struct mbedtls_cipher_base_t mbedtls_cipher_base_t;

typedef struct mbedtls_cipher_info_t {
    mbedtls_cipher_type_t type;
    mbedtls_cipher_mode_t mode;
    unsigned int key_bitlen;
    const char *name;
    int iv_size;
    int flags;
    unsigned int block_size;
    const mbedtls_cipher_base_t *base_idx;
} mbedtls_cipher_info_t;

typedef struct mbedtls_cipher_context_t {
    const mbedtls_cipher_info_t *cipher_info;
    int key_bitlen;
    mbedtls_operation_t operation;
    void *cipher_ctx;
    unsigned char iv[16];
    size_t iv_size;
    unsigned char unprocessed_data[16];
    size_t unprocessed_len;
} mbedtls_cipher_context_t;

struct mbedtls_cipher_base_t {
    mbedtls_cipher_type_t cipher;
    int (*ecb_func)(void *ctx, mbedtls_operation_t mode,
                    const unsigned char *input, unsigned char *output);
    int (*cbc_func)(void *ctx, mbedtls_operation_t mode, size_t length,
                    unsigned char *iv, const unsigned char *input,
                    unsigned char *output);
    int (*setkey_enc_func)(void *ctx, const unsigned char *key,
                          unsigned int key_bitlen);
    int (*setkey_dec_func)(void *ctx, const unsigned char *key,
                          unsigned int key_bitlen);
    void * (*ctx_alloc_func)(void);
    void (*ctx_free_func)(void *ctx);
};

extern const mbedtls_cipher_base_t mbedtls_aes_info;

#ifdef __cplusplus
extern "C" {
#endif

const mbedtls_cipher_info_t *mbedtls_cipher_info_from_type(const mbedtls_cipher_type_t cipher_type);

void mbedtls_cipher_init(mbedtls_cipher_context_t *ctx);
void mbedtls_cipher_free(mbedtls_cipher_context_t *ctx);

int mbedtls_cipher_setup(mbedtls_cipher_context_t *ctx, const mbedtls_cipher_info_t *cipher_info);
int mbedtls_cipher_setkey(mbedtls_cipher_context_t *ctx, const unsigned char *key,
                         int key_bitlen, const mbedtls_operation_t operation);

int mbedtls_cipher_set_iv(mbedtls_cipher_context_t *ctx, const unsigned char *iv, size_t iv_len);
int mbedtls_cipher_reset(mbedtls_cipher_context_t *ctx);

int mbedtls_cipher_update(mbedtls_cipher_context_t *ctx, const unsigned char *input,
                         size_t ilen, unsigned char *output, size_t *olen);
int mbedtls_cipher_finish(mbedtls_cipher_context_t *ctx, unsigned char *output, size_t *olen);

int mbedtls_cipher_crypt(mbedtls_cipher_context_t *ctx,
                        const unsigned char *iv, size_t iv_len,
                        const unsigned char *input, size_t ilen,
                        unsigned char *output, size_t *olen);

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_CIPHER_H */