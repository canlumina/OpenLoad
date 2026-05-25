/*
 * 用户工程配置 - 覆盖 OpenLoad 默认值
 *
 * 仅写自己关心的项, 其它项继承 openload/include/openload/config_default.h
 * 中的默认。
 */
#pragma once

/* 板子标识. 不同板子用不同 ID, 防止误刷. */
#define OPENLOAD_BOARD_ID               0x0103

/* 按键 + UART + magic 三类触发都启用 (默认即如此, 显式注明清晰) */
#define OPENLOAD_BOOT_DELAY_MS          3000

/* M1: XMODEM + 1K. M2: 加 YMODEM + HTTP OTA. */
#define OPENLOAD_ENABLE_XMODEM          1
#define OPENLOAD_ENABLE_XMODEM_1K       1
#define OPENLOAD_ENABLE_YMODEM          1
#define OPENLOAD_ENABLE_HTTP_OTA        1

/* M2-12: 启用 ESP8266 AT 桥接 (UART2 + AT 引擎). 跟 CMakeLists.txt 里的
 * STM32F1_PORT_ENABLE_ESP8266 必须保持一致 — 前者控 C 宏 (it.c/dma.c/dispatcher
 * 是否包含 UART2 分支), 后者控 port 库是否编入 port_uart2.c. */
#define OPENLOAD_ENABLE_ESP8266         1

/* M2-13: 持久化操作日志. 依赖 partitions.def 里的 "oplog" 分区. */
#define OPENLOAD_ENABLE_OPLOG           1

/* 升级策略: 外部 staging → 内部 app 覆盖 */
#define OPENLOAD_UPGRADE_STRATEGY       OL_UPGRADE_STAGING

/* M3-1: 启用防回滚. 拒绝 firmware_version 低于当前 app 的固件刷入.
 * CLI: install ... force 可单次绕过. */
#define OPENLOAD_ANTI_ROLLBACK          1

/* 日志 INFO 级 + ANSI 彩色 (使用 Tera Term/Xshell 终端时美观) */
#define OPENLOAD_LOG_LEVEL              3
#define OPENLOAD_LOG_COLOR              1

/* CLI 不设密码 (开发阶段). 量产时改为 "yourpass". */
#define OPENLOAD_CLI_PASSWORD           ((const char *)0)
