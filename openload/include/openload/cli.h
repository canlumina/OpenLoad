/*
 * OpenLoad - 命令行接口
 *
 * 命令通过链接段静态注册, 无需手工 init/register 函数。
 * 例:
 *     static int handle_jump(int argc, char **argv) { ... }
 *     OL_CMD_REGISTER("jump", "Verify and jump to app", handle_jump);
 */
#pragma once

#include <stdint.h>
#include "openload/ops/io_ops.h"

typedef int (*ol_cmd_handler_t)(int argc, char **argv);

typedef struct {
    const char        *name;     /* 命令字, 例如 "jump" */
    const char        *help;     /* 一行简介, 用于 help 列表 */
    ol_cmd_handler_t   handler;  /* 处理函数 */
} ol_cmd_t;

#define OL_CMD_REGISTER(_name, _help, _handler)                          \
    static const ol_cmd_t __ol_cmd_##_handler                            \
        __attribute__((used, section(".ol_cmds"))) = {                   \
            .name = (_name), .help = (_help), .handler = (_handler) }

/**
 * @brief 启动 CLI 主循环 (永不返回, 除非命令调用 ol_jump/ol_reboot).
 * @param  io  CLI 读写通道, 通常是 "console"
 */
void ol_cli_run(ol_io_dev_t *io);

/** 单次执行一行命令字符串 (空格分词), 主要供测试调用. */
int ol_cli_exec(const char *line);

/** 枚举全部已注册命令, 返回数量并通过参数返回数组指针. */
uint32_t ol_cmd_table(const ol_cmd_t **out);
