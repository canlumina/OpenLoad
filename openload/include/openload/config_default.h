/*
 * OpenLoad - 默认配置
 *
 * 全部条目用 #ifndef 包裹, 用户可在 openload_config.h 中覆盖任意项。
 * 修改本文件等同于改框架默认值; 项目级修改请去 openload_config.h。
 */
#pragma once

/* ============================================================
 *  升级策略常量 (前置定义, 让 OPENLOAD_UPGRADE_STRATEGY 检查能用)
 * ============================================================ */
#define OL_UPGRADE_SINGLE_BANK      1
#define OL_UPGRADE_STAGING          2
#define OL_UPGRADE_STAGING_BACKUP   3
#define OL_UPGRADE_DUAL_BANK        4

/* ============================================================
 *                       Boot
 * ============================================================ */

/** 等待启动触发的超时 (ms). 超时则尝试跳转 App. */
#ifndef OPENLOAD_BOOT_DELAY_MS
#define OPENLOAD_BOOT_DELAY_MS              3000
#endif

/** 触发源使能: 任一启用且条件满足即进入 CLI. */
#ifndef OPENLOAD_ENTRY_TRIGGER_BUTTON
#define OPENLOAD_ENTRY_TRIGGER_BUTTON       1
#endif
#ifndef OPENLOAD_ENTRY_TRIGGER_UART
#define OPENLOAD_ENTRY_TRIGGER_UART         1
#endif
#ifndef OPENLOAD_ENTRY_TRIGGER_MAGIC
#define OPENLOAD_ENTRY_TRIGGER_MAGIC        1
#endif

/** App 校验失败时的处理策略 */
#define OPENLOAD_RECOVERY_CLI               1   /* 失败 → 留在 CLI */
#define OPENLOAD_RECOVERY_HANG              2   /* 失败 → 死循环 (依赖看门狗复位) */
#define OPENLOAD_RECOVERY_ROLLBACK          3   /* 失败 → 自动从 backup 恢复 (要求 STAGING_BACKUP 策略) */
#ifndef OPENLOAD_RECOVERY_POLICY
#define OPENLOAD_RECOVERY_POLICY            OPENLOAD_RECOVERY_CLI
#endif

/* ============================================================
 *                     Receivers
 * ============================================================ */
#ifndef OPENLOAD_ENABLE_XMODEM
#define OPENLOAD_ENABLE_XMODEM              1
#endif
#ifndef OPENLOAD_ENABLE_XMODEM_1K
#define OPENLOAD_ENABLE_XMODEM_1K           1
#endif
#ifndef OPENLOAD_ENABLE_YMODEM
#define OPENLOAD_ENABLE_YMODEM              0   /* M2 */
#endif
#ifndef OPENLOAD_ENABLE_HTTP_OTA
#define OPENLOAD_ENABLE_HTTP_OTA            0   /* M2 */
#endif

/* ============================================================
 *                       Crypto
 * ============================================================ */
#ifndef OPENLOAD_ENABLE_CRC32
#define OPENLOAD_ENABLE_CRC32               1   /* 强制启用, 仅作可见性占位 */
#endif
#ifndef OPENLOAD_ENABLE_AES_128_CTR
#define OPENLOAD_ENABLE_AES_128_CTR         0   /* M3 */
#endif
#ifndef OPENLOAD_ENABLE_SHA256
#define OPENLOAD_ENABLE_SHA256              0   /* M3 */
#endif
#ifndef OPENLOAD_ENABLE_ED25519
#define OPENLOAD_ENABLE_ED25519             0   /* M4+ */
#endif

/* ============================================================
 *                     Image / Update
 * ============================================================ */
#ifndef OPENLOAD_UPGRADE_STRATEGY
#define OPENLOAD_UPGRADE_STRATEGY           OL_UPGRADE_STAGING
#endif

/** 防回滚: 1 = 拒绝低于当前版本的固件 (要求当前 App 已含有效 image header). */
#ifndef OPENLOAD_ANTI_ROLLBACK
#define OPENLOAD_ANTI_ROLLBACK              0
#endif

/** 板子 ID, 用于跨型号刷固件保护. 0 = 不检查. */
#ifndef OPENLOAD_BOARD_ID
#define OPENLOAD_BOARD_ID                   0x0001
#endif

/* ============================================================
 *                         CLI
 * ============================================================ */
#ifndef OPENLOAD_ENABLE_CLI
#define OPENLOAD_ENABLE_CLI                 1
#endif
#ifndef OPENLOAD_CLI_LINE_MAX
#define OPENLOAD_CLI_LINE_MAX               128
#endif
#ifndef OPENLOAD_CLI_PROMPT
#define OPENLOAD_CLI_PROMPT                 "OpenLoad> "
#endif
/** 设为字符串字面量 (例如 "mysecret") 启用 CLI 密码; NULL = 不启用. */
#ifndef OPENLOAD_CLI_PASSWORD
#define OPENLOAD_CLI_PASSWORD               ((const char *)0)
#endif

/* ============================================================
 *                        Logger
 * ============================================================ */
/* 1=ERR 2=WRN 3=INF 4=DBG 0=NONE */
#ifndef OPENLOAD_LOG_LEVEL
#define OPENLOAD_LOG_LEVEL                  3
#endif
#ifndef OPENLOAD_LOG_COLOR
#define OPENLOAD_LOG_COLOR                  1
#endif

/* ============================================================
 *                       Buffers
 * ============================================================ */
/** Staging → target 拷贝时的块大小 (字节). 越大越快但占 RAM. */
#ifndef OPENLOAD_COPY_CHUNK_SIZE
#define OPENLOAD_COPY_CHUNK_SIZE            512
#endif

/* ============================================================
 *                  互斥性 / 健全性检查
 * ============================================================ */
#if OPENLOAD_ENABLE_XMODEM_1K && !OPENLOAD_ENABLE_XMODEM
#  error "OPENLOAD_ENABLE_XMODEM_1K requires OPENLOAD_ENABLE_XMODEM"
#endif

#if (OPENLOAD_UPGRADE_STRATEGY != 1) && (OPENLOAD_UPGRADE_STRATEGY != 2) && \
    (OPENLOAD_UPGRADE_STRATEGY != 3) && (OPENLOAD_UPGRADE_STRATEGY != 4)
#  error "OPENLOAD_UPGRADE_STRATEGY must be one of OL_UPGRADE_*"
#endif
