#include "mbedtls/platform.h"
#include <stdlib.h>
#include <stdio.h>

#if defined(MBEDTLS_PLATFORM_MEMORY)
void * (*mbedtls_calloc)(size_t n, size_t size) = MBEDTLS_PLATFORM_STD_CALLOC;
void (*mbedtls_free)(void *ptr) = MBEDTLS_PLATFORM_STD_FREE;

int mbedtls_platform_set_calloc_free(void * (*calloc_func)(size_t, size_t),
                                     void (*free_func)(void *))
{
    mbedtls_calloc = calloc_func;
    mbedtls_free = free_func;
    return 0;
}
#endif /* MBEDTLS_PLATFORM_MEMORY */

#if defined(MBEDTLS_PLATFORM_PRINTF_ALT)
int (*mbedtls_printf)(const char *format, ...) = MBEDTLS_PLATFORM_STD_PRINTF;

int mbedtls_platform_set_printf(int (*printf_func)(const char *, ...))
{
    mbedtls_printf = printf_func;
    return 0;
}
#endif /* MBEDTLS_PLATFORM_PRINTF_ALT */

void mbedtls_platform_zeroize(void *buf, size_t len)
{
    volatile unsigned char *p = (volatile unsigned char *)buf;
    while (len--) {
        *p++ = 0;
    }
}