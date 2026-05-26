/*
 * 用户工程配置 - 覆盖 OpenLoad 默认值 (STM32F407VGT6, DevEBox)
 *
 * 仅写自己关心的项, 其它项继承 openload/include/openload/config_default.h
 * 中的默认。
 */
#pragma once

/* 板子标识. F4 用 0x0407 区分 F1 的 0x0103, 防止 image 跨板误刷. */
#define OPENLOAD_BOARD_ID               0x0407

/* 按键 + UART + magic 三类触发都启用 */
#define OPENLOAD_BOOT_DELAY_MS          3000

/* 协议矩阵 (跟 F103 一致): XMODEM + 1K + YMODEM + HTTP OTA */
#define OPENLOAD_ENABLE_XMODEM          1
#define OPENLOAD_ENABLE_XMODEM_1K       1
#define OPENLOAD_ENABLE_YMODEM          1
#define OPENLOAD_ENABLE_HTTP_OTA        1

/* ESP8266 AT 桥接 (UART2). 跟 CMakeLists.txt 的 STM32F4_PORT_ENABLE_ESP8266 保持一致. */
#define OPENLOAD_ENABLE_ESP8266         1

/* 持久化操作日志 (oplog 分区, W25Q16). */
#define OPENLOAD_ENABLE_OPLOG           1

/* 升级策略: 外部 staging → 内部 app 覆盖 */
#define OPENLOAD_UPGRADE_STRATEGY       OL_UPGRADE_STAGING

/* M3-1: 防回滚. */
#define OPENLOAD_ANTI_ROLLBACK          1

/* M3-2: backup/rollback. partitions.def 必须有 "backup". */
#define OPENLOAD_ENABLE_BACKUP          1

/* M3-4: AES-128-CTR image 解密. key 跟 F103 用同一个 demo key, 方便复用 image_tool 输出. */
#define OPENLOAD_ENABLE_AES_128_CTR     1
#define OPENLOAD_AES_KEY_BYTES \
    0x4F, 0x70, 0x65, 0x6E, 0x4C, 0x6F, 0x61, 0x64, \
    0x20, 0x64, 0x65, 0x6D, 0x6F, 0x4B, 0x65, 0x79

/* M4-1: SHA-256 摘要. */
#define OPENLOAD_ENABLE_SHA256          1

/* M4-2: Ed25519 签名. pubkey 跟 F103 共用 demo (tools/gen_demo_ed25519.py 生成). */
#define OPENLOAD_ENABLE_ED25519         1
#define OPENLOAD_ED25519_PUBKEY_BYTES \
    0x75,0x42,0xCE,0x6A,0xEC,0xF5,0xF1,0xE6, \
    0xDC,0x80,0x9E,0x5F,0x49,0xBA,0xFE,0xDB, \
    0x04,0x58,0x83,0xA6,0x3C,0x93,0x1D,0xFD, \
    0x2E,0x1E,0xCF,0xFC,0xC0,0x1F,0xDB,0x98

/* M6-1: STM32 RDP 软件控制 (L0→L1 lock). 启用后 sys_ops 注册 rdp_* op +
 * CLI 暴露 `rdp` / `rdp lock` 命令. 真正触发烧 option byte 必须 user 在 CLI
 * 二次 'y' 确认 — 不会主动锁. */
#define OPENLOAD_ENABLE_RDP             1

/* 日志 INFO 级 + ANSI 彩色 */
#define OPENLOAD_LOG_LEVEL              3
#define OPENLOAD_LOG_COLOR              1

/* CLI 不设密码 (开发阶段). */
#define OPENLOAD_CLI_PASSWORD           ((const char *)0)
