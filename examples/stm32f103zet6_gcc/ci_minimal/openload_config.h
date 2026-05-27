/*
 * OpenLoad CI - minimal 配置 (F1, M1 默认)
 *
 * 仅启用 M1 默认值: XMODEM + XMODEM-1K + CLI + CRC32.
 * 关闭 M2-M6 全部特性, 验证框架 #ifdef 裁剪正确.
 *
 * 通过 -DOPENLOAD_CI_MINIMAL=ON 由 example CMakeLists.txt 选用:
 * 把本目录加到 openload target 的 include path 并优先, shadow 掉
 * 主目录的 openload_config.h. 主目录文件不动.
 *
 * 不要把这个文件当成"生产用 minimal 模板" — 真用户应改主
 * openload_config.h. 本文件只服务 CI 矩阵的 "minimal" 变体.
 */
#pragma once

#define OPENLOAD_BOARD_ID               0x0103
#define OPENLOAD_BOOT_DELAY_MS          3000

/* M1: XMODEM + CRC32 + CLI (framework 默认即此, 显式列出以求清晰) */
#define OPENLOAD_ENABLE_XMODEM          1
#define OPENLOAD_ENABLE_XMODEM_1K       1

/* 显式关闭 M2-M6 全部特性 (框架默认即 0, 显式覆盖防止 future 默认值漂移) */
#define OPENLOAD_ENABLE_YMODEM          0
#define OPENLOAD_ENABLE_HTTP_OTA        0
#define OPENLOAD_ENABLE_ESP8266         0
#define OPENLOAD_ENABLE_OPLOG           0
#define OPENLOAD_ENABLE_AES_128_CTR     0
#define OPENLOAD_ENABLE_SHA256          0
#define OPENLOAD_ENABLE_ED25519         0
#define OPENLOAD_ANTI_ROLLBACK          0
#define OPENLOAD_ENABLE_BACKUP          0
#define OPENLOAD_ENABLE_RDP             0

#define OPENLOAD_UPGRADE_STRATEGY       OL_UPGRADE_STAGING
#define OPENLOAD_LOG_LEVEL              2   /* WARN+ (省 ROM) */
#define OPENLOAD_LOG_COLOR              0
#define OPENLOAD_CLI_PASSWORD           ((const char *)0)
