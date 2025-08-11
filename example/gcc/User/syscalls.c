/* Minimal newlib syscalls for printf redirection (GCC) */
#include "stm32f1xx.h" /* for USART register access */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

/* Choose which USART to use for stdout */
#ifndef STDOUT_USART
#define STDOUT_USART USART1
#endif

static void stdout_write_blocking(const char *data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        char ch = data[i];
        /* Convert LF to CRLF for terminals expecting CR+LF */
        if (ch == '\n') {
            while ((STDOUT_USART->SR & (uint16_t)0x0080) == 0) {}
            STDOUT_USART->DR = (uint8_t)'\r';
        }
        while ((STDOUT_USART->SR & (uint16_t)0x0080) == 0) {}
        STDOUT_USART->DR = (uint8_t)ch;
    }
    /* Wait for TC (Transmission complete) */
    while ((STDOUT_USART->SR & (uint16_t)0x0040) == 0) {
    }
}

int _write(int file, const char *ptr, int len) {
    (void)file;

    if (len <= 0 || ptr == NULL)
        return 0;
    stdout_write_blocking(ptr, (size_t)len);
    return len;
}

int _read(int file, char *ptr, int len) {
    (void)file;
    (void)ptr;
    (void)len;
    errno = ENOSYS;
    return -1;
}

int _close(int file) {
    (void)file;
    return -1;
}

int _fstat(int file, struct stat *st) {
    (void)file;
    if (st) {
        st->st_mode = S_IFCHR;
    }
    return 0;
}

int _isatty(int file) {
    (void)file;
    return 1;
}

int _lseek(int file, int ptr, int dir) {
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

/* Simple heap implementation */
extern char _end; /* from linker script */
static char *heap_end;
void *_sbrk(ptrdiff_t incr) {
    if (heap_end == NULL) {
        heap_end = &_end;
    }
    char *prev = heap_end;
    heap_end += incr;
    return prev;
}
