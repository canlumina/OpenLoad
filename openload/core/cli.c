/*
 * OpenLoad - CLI 主循环
 *
 * 简易行编辑 (回退 + 回车), 空格分词, 链接段枚举命令。
 * 不实现历史 / 自动补全 (节省 ROM)。
 */
#include "openload/cli.h"
#include "openload/logger.h"
#include "openload/config.h"
#include "openload/errno.h"
#include "openload/ops/sys_ops.h"
#include <string.h>
#include <stddef.h>

extern const ol_cmd_t __ol_cmds_start[];
extern const ol_cmd_t __ol_cmds_end[];

uint32_t ol_cmd_table(const ol_cmd_t **out)
{
    if (out) {
        *out = __ol_cmds_start;
    }
    return (uint32_t)(__ol_cmds_end - __ol_cmds_start);
}

static const ol_cmd_t *find_cmd(const char *name)
{
    for (const ol_cmd_t *c = __ol_cmds_start; c != __ol_cmds_end; ++c) {
        if (c->name && strcmp(c->name, name) == 0) {
            return c;
        }
    }
    return NULL;
}

/* 简易分词: 把行内空白替换为 \0, 输出 argv 指针数组 */
static int tokenize(char *line, char **argv, int max_argv)
{
    int argc = 0;
    char *p  = line;
    while (*p && argc < max_argv) {
        while (*p == ' ' || *p == '\t') { *p++ = '\0'; }
        if (!*p) { break; }
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') { p++; }
    }
    return argc;
}

int ol_cli_exec(const char *line)
{
    if (!line) { return OL_E_INVAL; }
    char  buf[OPENLOAD_CLI_LINE_MAX];
    char *argv[8];
    size_t n = strlen(line);
    if (n >= sizeof(buf)) {
        n = sizeof(buf) - 1;
    }
    memcpy(buf, line, n);
    buf[n] = '\0';
    int argc = tokenize(buf, argv, (int)(sizeof(argv) / sizeof(argv[0])));
    if (argc == 0) { return OL_OK; }
    const ol_cmd_t *cmd = find_cmd(argv[0]);
    if (!cmd) {
        ol_printf("unknown: %s\r\n", argv[0]);
        return OL_E_NOT_FOUND;
    }
    /* `<cmd> help` 由 dispatcher 直接答, 不进 handler.
     * 让所有命令零成本获得 `<cmd> help` 入口, 同时给没注册 long_help 的
     * 命令一个统一回退提示 (避免各 handler 自行 "usage: ..." 风格不一).
     * 副作用: handler 不能把 "help" 当业务参数 — 目前无此用法. */
    if (argc >= 2 && strcmp(argv[1], "help") == 0) {
        if (cmd->long_help) {
            ol_print(cmd->long_help);
        } else {
            ol_printf("  %-10s %s\r\n", cmd->name,
                      cmd->help ? cmd->help : "");
            ol_print("(no detailed help)\r\n");
        }
        return OL_OK;
    }
    return cmd->handler(argc, argv);
}

/* 可选口令保护. 进入主循环前阻塞至口令正确.
   设置 OPENLOAD_CLI_PASSWORD 为非空字符串启用; NULL 或空串则跳过.
   失败 OPENLOAD_CLI_PASSWORD_MAX_ATTEMPTS 次后锁定 LOCKOUT_MS 期间不接受
   任何输入. 失败 / 锁定事件走 LOGW, 配合 M2-13 oplog hook 自动持久化,
   方便事后排查物理接入攻击. */
static void prompt_password(ol_io_dev_t *io)
{
    const char *want = OPENLOAD_CLI_PASSWORD;
    if (!want || !want[0]) {
        return;
    }
    uint32_t fails = 0;
    char in[64];
    while (1) {
        ol_print("password: ");
        size_t pos = 0;
        while (1) {
            uint8_t c;
            if (ol_io_getc_timeout(io, &c, 0xFFFFFFFFu) != OL_OK) { continue; }
            if (c == '\r' || c == '\n') { break; }
            if (c == 0x7F || c == 0x08) {
                if (pos > 0) {
                    pos--;
                    ol_print("\b \b");
                }
                continue;
            }
            /* 仅可打印字符接受, 回显 '*' 给用户击键反馈. */
            if (c >= 0x20 && c < 0x7F && pos < sizeof(in) - 1) {
                in[pos++] = (char)c;
                ol_io_putc(io, '*');
            }
        }
        in[pos] = '\0';
        ol_print("\r\n");

        if (strcmp(in, want) == 0) {
            if (fails) {
                OL_LOGW("cli: password ok after %lu fail(s)", (uint32_t)fails);
            }
            return;
        }

        fails++;
        OL_LOGW("cli: password fail %lu/%u",
                (uint32_t)fails,
                (unsigned)OPENLOAD_CLI_PASSWORD_MAX_ATTEMPTS);

        if (fails >= OPENLOAD_CLI_PASSWORD_MAX_ATTEMPTS) {
            ol_printf("locked %lu s\r\n",
                      (uint32_t)(OPENLOAD_CLI_PASSWORD_LOCKOUT_MS / 1000u));
            OL_LOGW("cli: locked %lu ms",
                    (uint32_t)OPENLOAD_CLI_PASSWORD_LOCKOUT_MS);
            uint32_t start = ol_tick_ms();
            while ((ol_tick_ms() - start) < OPENLOAD_CLI_PASSWORD_LOCKOUT_MS) {
                /* 期间持续清空输入, 避免缓冲积压在解锁瞬间被当成密码. */
                uint8_t c;
                (void)ol_io_getc_timeout(io, &c, 100);
            }
            fails = 0;
        } else {
            ol_print("denied\r\n");
        }
    }
}

void ol_cli_run(ol_io_dev_t *io)
{
    if (!io) { return; }
    prompt_password(io);
    char line[OPENLOAD_CLI_LINE_MAX];

    for (;;) {
        ol_print(OPENLOAD_CLI_PROMPT);
        size_t pos = 0;
        while (pos < sizeof(line) - 1) {
            uint8_t c;
            if (ol_io_getc_timeout(io, &c, 0xFFFFFFFFu) != OL_OK) {
                continue;
            }
            if (c == '\r' || c == '\n') {
                ol_print("\r\n");
                break;
            }
            /* DEL / BS */
            if (c == 0x7F || c == 0x08) {
                if (pos > 0) {
                    pos--;
                    ol_print("\b \b");
                }
                continue;
            }
            /* 可打印字符 echo */
            if (c >= 0x20 && c < 0x7F) {
                line[pos++] = (char)c;
                ol_io_putc(io, c);
            }
        }
        line[pos] = '\0';
        ol_cli_exec(line);
    }
}
