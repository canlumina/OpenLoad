#ifndef MBEDTLS_CHECK_CONFIG_H
#define MBEDTLS_CHECK_CONFIG_H

/* 简化的配置检查 - 只进行基本的依赖检查 */

#if defined(MBEDTLS_AES_C) && !defined(MBEDTLS_CIPHER_C)
#error "MBEDTLS_AES_C defined, but not MBEDTLS_CIPHER_C"
#endif

#if defined(MBEDTLS_CIPHER_MODE_CBC) && !defined(MBEDTLS_CIPHER_C)
#error "MBEDTLS_CIPHER_MODE_CBC defined, but not MBEDTLS_CIPHER_C"
#endif

#if defined(MBEDTLS_CTR_DRBG_C) && !defined(MBEDTLS_AES_C)
#error "MBEDTLS_CTR_DRBG_C defined, but not MBEDTLS_AES_C"
#endif

#endif /* MBEDTLS_CHECK_CONFIG_H */