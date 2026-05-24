/*
 * OpenLoad - Logger 实现
 *
 * 自带精简 vsnprintf 实现, 不依赖 newlib stdio (newlib_nano 的 printf 仍要
 * 接近 6-8 KB), 支持: %s %c %d %i %u %x %X %p %% 与可选宽度 %5d / %08x.
 *
 * 所有日志通过 ol_io_dev_find("console") 输出; console 未注册时静默丢弃。
 */
#include "openload/logger.h"
#include "openload/ops/io_ops.h"
#include "openload/config.h"
#if OPENLOAD_ENABLE_OPLOG
#  include "openload/oplog.h"
#endif
#include <stdarg.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

#ifndef OPENLOAD_LOG_BUF_SIZE
#define OPENLOAD_LOG_BUF_SIZE   192
#endif

static ol_log_level_t s_level = (ol_log_level_t)OPENLOAD_LOG_LEVEL;

void ol_log_set_level(ol_log_level_t lvl) { s_level = lvl; }
ol_log_level_t ol_log_get_level(void)     { return s_level; }

/* -------- 精简 vsnprintf -------- */

static int emit_pad(char *dst, int cap, int pos, char pad, int n)
{
    while (n-- > 0 && pos < cap) {
        dst[pos++] = pad;
    }
    return pos;
}

static int emit_str(char *dst, int cap, int pos, const char *s, int width, int leftjust, char pad)
{
    if (!s) { s = "(null)"; }
    int len = (int)strlen(s);
    if (!leftjust && width > len) {
        pos = emit_pad(dst, cap, pos, pad, width - len);
    }
    while (*s && pos < cap) {
        dst[pos++] = *s++;
    }
    if (leftjust && width > len) {
        pos = emit_pad(dst, cap, pos, ' ', width - len);
    }
    return pos;
}

static int emit_uint(char *dst, int cap, int pos, uint32_t v, uint32_t base,
                     int upper, int width, int leftjust, char pad)
{
    char tmp[12];
    int  n = 0;
    if (v == 0) {
        tmp[n++] = '0';
    } else {
        while (v && n < (int)sizeof(tmp)) {
            uint32_t d = v % base;
            tmp[n++]   = (char)(d < 10 ? '0' + d : (upper ? 'A' : 'a') + (d - 10));
            v         /= base;
        }
    }
    if (!leftjust && width > n) {
        pos = emit_pad(dst, cap, pos, pad, width - n);
    }
    while (n-- > 0 && pos < cap) {
        dst[pos++] = tmp[n];
    }
    if (leftjust && width > n) {
        pos = emit_pad(dst, cap, pos, ' ', width - n);
    }
    return pos;
}

static int ol_vsnprintf(char *dst, int cap, const char *fmt, va_list ap)
{
    int pos = 0;
    while (*fmt && pos < cap - 1) {
        if (*fmt != '%') {
            dst[pos++] = *fmt++;
            continue;
        }
        fmt++;
        int  leftjust = 0;
        char pad      = ' ';
        if (*fmt == '-') { leftjust = 1; fmt++; }
        if (*fmt == '0') { pad      = '0'; fmt++; }
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        /* 长度修饰 (l/z) 忽略, bootloader 不处理 64-bit */
        if (*fmt == 'l' || *fmt == 'z') { fmt++; }

        switch (*fmt) {
        case 's': pos = emit_str(dst, cap, pos, va_arg(ap, const char *), width, leftjust, pad); break;
        case 'c': {
            char c = (char)va_arg(ap, int);
            if (pos < cap) { dst[pos++] = c; }
            break;
        }
        case 'd':
        case 'i': {
            int32_t v = va_arg(ap, int32_t);
            if (v < 0) {
                if (pos < cap) { dst[pos++] = '-'; }
                v = -v;
                if (width > 0) { width--; }
            }
            pos = emit_uint(dst, cap, pos, (uint32_t)v, 10, 0, width, leftjust, pad);
            break;
        }
        case 'u':
            pos = emit_uint(dst, cap, pos, va_arg(ap, uint32_t), 10, 0, width, leftjust, pad);
            break;
        case 'x':
            pos = emit_uint(dst, cap, pos, va_arg(ap, uint32_t), 16, 0, width, leftjust, pad);
            break;
        case 'X':
            pos = emit_uint(dst, cap, pos, va_arg(ap, uint32_t), 16, 1, width, leftjust, pad);
            break;
        case 'p': {
            if (pos + 1 < cap) { dst[pos++] = '0'; dst[pos++] = 'x'; }
            pos = emit_uint(dst, cap, pos, (uint32_t)(uintptr_t)va_arg(ap, void *), 16, 0, 8, 0, '0');
            break;
        }
        case '%':
            if (pos < cap) { dst[pos++] = '%'; }
            break;
        default:
            if (pos < cap) { dst[pos++] = '%'; }
            if (pos < cap) { dst[pos++] = *fmt; }
            break;
        }
        fmt++;
    }
    dst[pos] = '\0';
    return pos;
}

/* -------- 输出 -------- */

static void write_console(const char *buf, int len)
{
    ol_io_dev_t *io = ol_io_dev_find("console");
    if (!io || !io->ops || !io->ops->write || len <= 0) {
        return;
    }
    io->ops->write(io, (const uint8_t *)buf, (uint32_t)len);
}

int ol_print(const char *s)
{
    if (!s) { return 0; }
    int len = (int)strlen(s);
    write_console(s, len);
    return len;
}

int ol_printf(const char *fmt, ...)
{
    char    buf[OPENLOAD_LOG_BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    int n = ol_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    write_console(buf, n);
    return n;
}

void ol_vlog(ol_log_level_t lvl, const char *fmt, va_list ap)
{
    if (lvl > s_level || lvl == OL_LOG_NONE) {
        return;
    }
    static const char *tag[] = { "", "E", "W", "I", "D" };
#if OPENLOAD_LOG_COLOR
    static const char *clr[] = { "", "\x1b[31m", "\x1b[33m", "\x1b[32m", "\x1b[36m" };
    static const char *rst   = "\x1b[0m";
#endif

    /* 先格式化裸消息 (无前缀/无颜色/无换行), 供 oplog 入盘使用; 控制台输出
     * 在这个基础上再加装饰. */
    char raw[OPENLOAD_LOG_BUF_SIZE];
    int  raw_len = ol_vsnprintf(raw, sizeof(raw), fmt, ap);

    char buf[OPENLOAD_LOG_BUF_SIZE + 16];
    int  pos = 0;
#if OPENLOAD_LOG_COLOR
    const char *c = clr[lvl];
    while (*c && pos < (int)sizeof(buf)) { buf[pos++] = *c++; }
#endif
    if (pos + 4 < (int)sizeof(buf)) {
        buf[pos++] = '[';
        buf[pos++] = tag[lvl][0];
        buf[pos++] = ']';
        buf[pos++] = ' ';
    }
    for (int i = 0; i < raw_len && pos < (int)sizeof(buf); ++i) {
        buf[pos++] = raw[i];
    }
#if OPENLOAD_LOG_COLOR
    const char *r = rst;
    while (*r && pos < (int)sizeof(buf) - 2) { buf[pos++] = *r++; }
#endif
    if (pos + 2 < (int)sizeof(buf)) {
        buf[pos++] = '\r';
        buf[pos++] = '\n';
    }
    write_console(buf, pos);

#if OPENLOAD_ENABLE_OPLOG
    /* 仅 ERR/WRN 自动入盘, 避免 INFO 风暴拖慢 SPI flash 写 (单次 ~1ms).
     * 显式入盘走 ol_oplog_append. */
    if (lvl == OL_LOG_ERR || lvl == OL_LOG_WRN) {
        ol_oplog_append((uint8_t)lvl, raw, (uint32_t)raw_len);
    }
#endif
}

void ol_log(ol_log_level_t lvl, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    ol_vlog(lvl, fmt, ap);
    va_end(ap);
}
