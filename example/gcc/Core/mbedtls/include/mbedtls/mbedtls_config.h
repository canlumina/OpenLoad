#ifndef MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

/* 系统和平台支持 */
#define MBEDTLS_HAVE_ASM
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY

/* 基础功能 */
#define MBEDTLS_AES_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_ENTROPY_C

/* 加密模式 */
#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_CIPHER_MODE_CTR

/* 优化配置 - 为STM32F103优化 */
#define MBEDTLS_AES_ROM_TABLES        /* 使用ROM中的S盒，节省RAM */
#define MBEDTLS_AES_FEWER_TABLES      /* 减少查找表大小 */

/* 内存和性能优化 */
#define MBEDTLS_MEMORY_BUFFER_ALLOC_C
#define MBEDTLS_TIMING_C

/* 禁用不需要的功能 */
#undef MBEDTLS_FS_IO
#undef MBEDTLS_NET_C
#undef MBEDTLS_TIMING_ALT

/* 错误报告 */
#define MBEDTLS_ERROR_C

/* 平台适配 */
#define MBEDTLS_PLATFORM_STD_CALLOC   calloc
#define MBEDTLS_PLATFORM_STD_FREE     free
#define MBEDTLS_PLATFORM_STD_PRINTF   printf
#define MBEDTLS_PLATFORM_STD_FPRINTF  fprintf

/* 包含检查 */
#include "mbedtls_check_config.h"

#endif /* MBEDTLS_CONFIG_H */