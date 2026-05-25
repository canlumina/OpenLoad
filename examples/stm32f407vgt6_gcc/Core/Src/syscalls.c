/*
 * Minimal syscall stubs for newlib-nano.
 *
 * OpenLoad bootloader 不用 printf/malloc/fopen 等; 这些 stub 仅用于让链接通过.
 * 任何被实际调用都会进 while(1)。
 */
#include <sys/stat.h>
#include <stdint.h>
#include <errno.h>

#undef errno
extern int errno;

int _write(int fd, char *buf, int len) { (void)fd; (void)buf; (void)len; return -1; }
int _read(int fd, char *buf, int len)  { (void)fd; (void)buf; (void)len; return -1; }
int _close(int fd)                     { (void)fd; return -1; }
int _fstat(int fd, struct stat *st)    { (void)fd; (void)st; return -1; }
int _isatty(int fd)                    { (void)fd; return 0; }
int _lseek(int fd, int p, int w)       { (void)fd; (void)p; (void)w; return -1; }

/* sbrk: 不用堆, 任何调用都失败. */
void *_sbrk(int incr) { (void)incr; errno = ENOMEM; return (void *)-1; }

int _getpid(void)                   { return 1; }
int _kill(int pid, int sig)         { (void)pid; (void)sig; errno = EINVAL; return -1; }
void _exit(int s)                   { (void)s; while (1) { } }
