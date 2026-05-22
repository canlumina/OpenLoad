#ifndef MBEDTLS_PLATFORM_H
#define MBEDTLS_PLATFORM_H

#include "mbedtls/mbedtls_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(MBEDTLS_PLATFORM_MEMORY)
extern void * (*mbedtls_calloc)(size_t n, size_t size);
extern void (*mbedtls_free)(void *ptr);

int mbedtls_platform_set_calloc_free(void * (*calloc_func)(size_t, size_t),
                                     void (*free_func)(void *));
#else
#define mbedtls_calloc    calloc
#define mbedtls_free      free
#endif /* MBEDTLS_PLATFORM_MEMORY */

#if defined(MBEDTLS_PLATFORM_PRINTF_ALT)
extern int (*mbedtls_printf)(const char *format, ...);
int mbedtls_platform_set_printf(int (*printf_func)(const char *, ...));
#else
#define mbedtls_printf    printf
#endif

void mbedtls_platform_zeroize(void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_PLATFORM_H */