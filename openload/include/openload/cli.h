/*
 * OpenLoad - 命令行接口
 *
 * 命令通过链接段静态注册, 无需手工 init/register 函数。
 * 例:
 *     static int handle_jump(int argc, char **argv) { ... }
 *     OL_CMD_REGISTER("jump", "Verify and jump to app", handle_jump);
 *
 * 子命令组类命令可用 OL_CMD_REGISTER_FULL 顺带挂一段 long_help; CLI 在
 * `help <cmd>` 或 `<cmd> help` 时打它, 比手编 argc<2 usage 行风格统一.
 */
#pragma once

#include <stdint.h>
#include "openload/ops/io_ops.h"

typedef int (*ol_cmd_handler_t)(int argc, char **argv);

typedef struct {
    const char        *name;       /* 命令字, 例如 "jump" */
    const char        *help;       /* 一行简介, help 列表用 */
    const char        *long_help;  /* 详细说明, `help <name>` / `<name> help` 用; NULL 表示未提供 */
    ol_cmd_handler_t   handler;    /* 处理函数 */
} ol_cmd_t;

/* 三参注册: 不带 long_help, 等同 OL_CMD_REGISTER_FULL(..., NULL, ...).
 * designated initializer 让未提及的 .long_help 自动 0 = NULL, 老代码无需改. */
#define OL_CMD_REGISTER(_name, _help, _handler)                          \
    static const ol_cmd_t __ol_cmd_##_handler                            \
        __attribute__((used, section(".ol_cmds"))) = {                   \
            .name = (_name), .help = (_help), .handler = (_handler) }

/* 四参注册: 多挂一段 long_help 字符串 (建议用 static const char[] 让编译器
 * 把它放 .rodata 而不是栈/.data). */
#define OL_CMD_REGISTER_FULL(_name, _help, _long_help, _handler)         \
    static const ol_cmd_t __ol_cmd_##_handler                            \
        __attribute__((used, section(".ol_cmds"))) = {                   \
            .name = (_name), .help = (_help),                            \
            .long_help = (_long_help), .handler = (_handler) }

/**
 * @brief 启动 CLI 主循环 (永不返回, 除非命令调用 ol_jump/ol_reboot).
 * @param  io  CLI 读写通道, 通常是 "console"
 */
void ol_cli_run(ol_io_dev_t *io);

/** 单次执行一行命令字符串 (空格分词), 主要供测试调用. */
int ol_cli_exec(const char *line);

/** 枚举全部已注册命令, 返回数量并通过参数返回数组指针. */
uint32_t ol_cmd_table(const ol_cmd_t **out);
