/*
 * OpenLoad - Boot Manager
 *
 * 启动状态机入口。典型 main:
 *     board_init();          // 用户提供 - 时钟、外设、注册 ops
 *     ol_boot_init();
 *     ol_boot_run();         // never return
 */
#pragma once

#include <stdint.h>

/**
 * @brief 框架初始化. 必须在所有 ops 注册完成之后、ol_boot_run 之前调用.
 *        会做: 校验必填 ops、初始化 logger console、扫描已注册分区。
 * @return OL_OK 或 < 0 错误码 (缺失必填 ops 时).
 */
int ol_boot_init(void);

/**
 * @brief 主状态机, 永不返回. 内部完成: 启动决策 → CLI/UPDATE → VERIFY → JUMP.
 */
void ol_boot_run(void) __attribute__((noreturn));

/**
 * @brief 校验并跳转到指定 App 分区. 失败返回, 成功永不返回.
 *        会执行: ol_image_verify → ol_disable_irq → flush console → ol_jump.
 */
int ol_boot_jump_to(const char *app_partition_name);

/**
 * @brief 等待启动触发, 阻塞至超时或检测到触发源.
 * @return 1 = 检测到触发 (进入 CLI), 0 = 超时.
 */
int ol_boot_wait_trigger(uint32_t timeout_ms);
