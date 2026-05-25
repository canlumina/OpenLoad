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

/* M3-2: 启用 backup / rollback. install 前先备份现 app -> backup 分区,
 * install 失败自动回滚; boot 检测到 INSTALLING magic 或 app verify 失败
 * 也自动回滚. partitions.def 必须含 "backup" 分区. */
#define OPENLOAD_ENABLE_BACKUP          1

/* M3-4: 启用 AES-128-CTR 解密. 加密 image 通过 image_tool.py --aes-key
 * 生成. Key 编进 bootloader (下方 OPENLOAD_AES_KEY_BYTES). 现阶段 key
 * 暴露在 bootloader 二进制里, 反向工程可取出; M3+ 可叠 OTP/RDP 升级. */
#define OPENLOAD_ENABLE_AES_128_CTR     1

/* M4-1: SHA-256 摘要. 当 hdr.firmware_sha256 非全 0 时在 ol_image_verify
 * 内追加 SHA-256 校验 (CRC32 之后). image_tool.py 默认对明文 payload
 * 算 SHA, 截前 16 字节 (128-bit 抗碰撞) 写入头. 加密 image 时 SHA 也是
 * 明文 SHA, 跟 firmware_crc32 同语义; install 解密回写 hdr 时 SHA 字段
 * 原样保留, 解密后 target verify 自动覆盖到 SHA 校验. */
#define OPENLOAD_ENABLE_SHA256          1

/* 16 字节 AES-128 密钥. 跟 image_tool.py --aes-key 必须一致.
 * 测试默认值: "OpenLoad demoKey" → 0x4F,0x70,...
 * 量产必须改成项目随机生成, 不要泄漏. */
#define OPENLOAD_AES_KEY_BYTES \
    0x4F, 0x70, 0x65, 0x6E, 0x4C, 0x6F, 0x61, 0x64, \
    0x20, 0x64, 0x65, 0x6D, 0x6F, 0x4B, 0x65, 0x79

/* 日志 INFO 级 + ANSI 彩色 (使用 Tera Term/Xshell 终端时美观) */
#define OPENLOAD_LOG_LEVEL              3
#define OPENLOAD_LOG_COLOR              1

/* CLI 不设密码 (开发阶段). 量产时改为 "yourpass". */
#define OPENLOAD_CLI_PASSWORD           ((const char *)0)
