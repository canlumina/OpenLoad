/*
 * OpenLoad - Logger
 *
 * 多级别日志, 编译期可裁剪。日志输出走 ol_io_dev_find("console"),
 * 若未注册 console 则静默丢弃。
 */
#pragma once

#include <stdarg.h>

typedef enum {
    OL_LOG_NONE = 0,
    OL_LOG_ERR  = 1,
    OL_LOG_WRN  = 2,
    OL_LOG_INF  = 3,
    OL_LOG_DBG  = 4,
} ol_log_level_t;

/** 运行时调整日志级别 (不超过编译期最大级别). */
void ol_log_set_level(ol_log_level_t lvl);
ol_log_level_t ol_log_get_level(void);

/** 主日志函数 (printf 风格). */
void ol_log(ol_log_level_t lvl, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
void ol_vlog(ol_log_level_t lvl, const char *fmt, va_list ap);

#include "openload/config.h"

/* 编译期裁剪宏: OPENLOAD_LOG_LEVEL 控制最高启用级别. */
#if OPENLOAD_LOG_LEVEL >= 1
#  define OL_LOGE(...) ol_log(OL_LOG_ERR, __VA_ARGS__)
#else
#  define OL_LOGE(...) ((void)0)
#endif

#if OPENLOAD_LOG_LEVEL >= 2
#  define OL_LOGW(...) ol_log(OL_LOG_WRN, __VA_ARGS__)
#else
#  define OL_LOGW(...) ((void)0)
#endif

#if OPENLOAD_LOG_LEVEL >= 3
#  define OL_LOGI(...) ol_log(OL_LOG_INF, __VA_ARGS__)
#else
#  define OL_LOGI(...) ((void)0)
#endif

#if OPENLOAD_LOG_LEVEL >= 4
#  define OL_LOGD(...) ol_log(OL_LOG_DBG, __VA_ARGS__)
#else
#  define OL_LOGD(...) ((void)0)
#endif

/** 不带级别的原始输出 (CLI 提示符、命令应答等). */
int ol_print(const char *s);
int ol_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
